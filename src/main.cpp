#include <SDL2/SDL_events.h>
#include <cstddef>
#include <cstring>
#include <fmt/core.h>
#include <fmt/ranges.h>
#include <span>
#include <stdexcept>
#include <vector>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_enums.hpp>
#include <vulkan/vulkan_raii.hpp>
#include <vulkan/vulkan_structs.hpp>

#include "context.hpp"
#include "vertex.hpp"
#include "win_setup.hpp"

namespace {
uint32_t findMemoryType(Context &c, uint32_t typeFilter,
                        vk::MemoryPropertyFlags properties) {
  auto memProperties = c.phys.getMemoryProperties();
  for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
    if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags &
                                    properties) == properties) {
      return i;
    }
  }

  throw std::runtime_error("failed to find suitable memory type!");
}

void vkassert(vk::Result r) {
  if (r != vk::Result::eSuccess)
    throw std::runtime_error(vk::to_string(r));
}

struct Buffer {
  vk::raii::Buffer buffer;
  vk::raii::DeviceMemory mem;
  Buffer(Context &c, size_t size, vk::BufferUsageFlags usage,
         vk::MemoryPropertyFlags properties)
      : buffer{nullptr}, mem{nullptr} {
    buffer = c.device.createBuffer(
        {.size = size,
         .usage = usage | vk::BufferUsageFlagBits::eTransferDst,
         .sharingMode = vk::SharingMode::eExclusive});
    auto mem_reqs = buffer.getMemoryRequirements();

    mem =
        c.device.allocateMemory({.allocationSize = mem_reqs.size,
                                 .memoryTypeIndex = findMemoryType(
                                     c, mem_reqs.memoryTypeBits, properties)});
    buffer.bindMemory(*mem, 0);
  }

  void write(Context &vk, std::span<const uint8_t> data) {

    using enum vk::BufferUsageFlagBits;
    using enum vk::MemoryPropertyFlagBits;
    auto staging =
        Buffer(vk, data.size(), eTransferSrc, eHostVisible | eHostCoherent);

    auto ptr = staging.mem.mapMemory(0, data.size());
    std::memcpy(ptr, data.data(), data.size());
    staging.mem.unmapMemory();

    auto cmdBuffers = vk.getCommands(1);
    vk::CommandBufferBeginInfo info{};
    vkassert(cmdBuffers[0].begin(&info));
    cmdBuffers[0].copyBuffer(*staging.buffer, *buffer,
                             vk::BufferCopy{0, 0, data.size()});
    cmdBuffers[0].end();
    vk.queues.mem().submit(std::array{vk::SubmitInfo{
        .commandBufferCount = 1, .pCommandBuffers = cmdBuffers.data()}});
  }
};
const std::vector<Vertex> vertices = {{{-0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}},
                                      {{0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}},
                                      {{0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}},
                                      {{-0.5f, 0.5f}, {1.0f, 1.0f, 1.0f}}};
const std::vector<uint16_t> indices = {0, 1, 2, 2, 3, 0};
template <typename T> std::span<const uint8_t> bin(T in) {
  return {reinterpret_cast<const uint8_t *>(in.data()),
          in.size() * sizeof(in[0])};
}

bool processInput() {
  SDL_Event event;
  SDL_PollEvent(&event);
  return event.type == SDL_QUIT ||
         (event.type == SDL_KEYDOWN &&
          event.key.keysym.scancode == SDL_SCANCODE_C &&
          event.key.keysym.mod & KMOD_CTRL);
}

void setScissorViewport(Context &c, vk::CommandBuffer buffer) {

  buffer.setViewport(
      0, std::array{vk::Viewport{
             .x = 0,
             .y = 0,
             .width = static_cast<float>(c.swapchain_extent.width),
             .height = static_cast<float>(c.swapchain_extent.height),
             .minDepth = 0,
             .maxDepth = 1.0}});

  buffer.setScissor(0, std::array{vk::Rect2D{.offset = {0, 0},
                                             .extent = c.swapchain_extent}});
}

void render(Context &c, std::span<vk::CommandBuffer> buffers, int index,
            Buffer &vert, Buffer &ind) {
  for (auto &buffer : buffers) {
    vk::CommandBufferBeginInfo info{};
    vkassert(buffer.begin(&info));
    vk::ClearValue clearColor = {.color = {std::array{0.0f, 0.0f, 0.0f, 1.0f}}};
    buffer.beginRenderPass({.renderPass = *c.pass,
                            .framebuffer = *c.framebuffers[index],
                            .renderArea = {{0, 0}, c.swapchain_extent},
                            .clearValueCount = 1,
                            .pClearValues = &clearColor},
                           vk::SubpassContents::eInline);
    buffer.bindPipeline(vk::PipelineBindPoint::eGraphics, *c.pipeline);

    buffer.bindVertexBuffers(0, std::array{*vert.buffer},
                             std::array{vk::DeviceSize(0)});
    buffer.bindIndexBuffer(*ind.buffer, 0, vk::IndexType::eUint16);
    setScissorViewport(c, buffer);
    buffer.drawIndexed(indices.size(), 1, 0, 0, 0);
    buffer.endRenderPass();
    buffer.end();
  }
}

void draw(Context &c, std::span<vk::CommandBuffer> buffers, Buffer &vert,
          Buffer &ind) {
  vkassert(
      c.device.waitForFences(std::array{*c.inflight_fen}, true, UINT64_MAX));
  c.device.resetFences(std::array{*c.inflight_fen});

  auto [result, imageIndex] =
      c.swapchain.acquireNextImage(UINT64_MAX, *c.image_available_sem);
  c.pool.reset();
  render(c, buffers, imageIndex, vert, ind);

  vk::Semaphore waitSemaphores[] = {*c.image_available_sem};
  vk::PipelineStageFlags waitStages[] = {
      vk::PipelineStageFlagBits::eColorAttachmentOutput};
  vk::Semaphore signalSemaphores[] = {*c.render_done_sem};
  std::array submit = {vk::SubmitInfo{.waitSemaphoreCount = 1,
                                      .pWaitSemaphores = waitSemaphores,
                                      .pWaitDstStageMask = waitStages,
                                      .commandBufferCount = 1,
                                      .pCommandBuffers = buffers.data(),
                                      .signalSemaphoreCount = 1,
                                      .pSignalSemaphores = signalSemaphores}};
  c.queues.render().submit(submit, *c.inflight_fen);

  vk::PresentInfoKHR presentInfo{};
  vk::SwapchainKHR swapChains[] = {*c.swapchain};
  presentInfo.waitSemaphoreCount = 1;
  presentInfo.pWaitSemaphores = signalSemaphores;
  presentInfo.swapchainCount = 1;
  presentInfo.pSwapchains = swapChains;
  presentInfo.pImageIndices = &imageIndex;
  vkassert(c.queues.render().presentKHR(presentInfo));
}
Buffer createVertBuffer(Context &vk) {
  using enum vk::BufferUsageFlagBits;
  using enum vk::MemoryPropertyFlagBits;
  auto vert = Buffer(vk, vertices.size() * sizeof(vertices[0]), eVertexBuffer,
                     eDeviceLocal);
  vert.write(vk, bin(vertices));

  return vert;
}
Buffer createIndBuffer(Context &vk) {
  using enum vk::BufferUsageFlagBits;
  using enum vk::MemoryPropertyFlagBits;
  auto vert = Buffer(vk, indices.size() * sizeof(indices[0]), eIndexBuffer,
                     eDeviceLocal);
  vert.write(vk, bin(indices));

  return vert;
}
} // namespace

int main() {
  auto vk = Context(Window("triangles!", {.width = 600, .height = 800}));
  auto cmdBuffers = vk.getCommands(1);
  auto vert = createVertBuffer(vk);
  auto ind = createIndBuffer(vk);
  vk.queues.mem().waitIdle();
  while (!processInput()) {
    draw(vk, cmdBuffers, vert, ind);
  }
  vk.device.waitIdle();
  return 0;
}