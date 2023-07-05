#include <SDL2/SDL_events.h>
#include <chrono>
#include <cstddef>
#include <cstring>
#include <fmt/core.h>
#include <fmt/ranges.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <ranges>
#include <span>
#include <stdexcept>
#include <vector>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_enums.hpp>
#include <vulkan/vulkan_raii.hpp>
#include <vulkan/vulkan_structs.hpp>

#include "context.hpp"
#include "ubo.hpp"
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
  vk::Buffer buffer{};
  vk::DeviceMemory mem{};
  vk::Device d{};
  Buffer(Context &c, size_t size, vk::BufferUsageFlags usage,
         vk::MemoryPropertyFlags properties) {
    d = *c.device;
    buffer = d.createBuffer({.size = size,
                             .usage = usage,
                             .sharingMode = vk::SharingMode::eExclusive});
    auto mem_reqs = d.getBufferMemoryRequirements(buffer);

    mem = d.allocateMemory({.allocationSize = mem_reqs.size,
                            .memoryTypeIndex = findMemoryType(
                                c, mem_reqs.memoryTypeBits, properties)});
    d.bindBufferMemory(buffer, mem, 0);
  }

  Buffer(Buffer &&other) {
    buffer = other.buffer;
    mem = other.mem;
    d = other.d;
    other.buffer = nullptr;
    other.mem = nullptr;
    other.d = nullptr;
  }
  ~Buffer() {
    if (d) {
      d.destroyBuffer(buffer);
      d.freeMemory(mem);
    }
  }

  void write(Context &vk, std::span<const uint8_t> data) {

    using enum vk::BufferUsageFlagBits;
    using enum vk::MemoryPropertyFlagBits;
    auto staging =
        Buffer(vk, data.size(), eTransferSrc, eHostVisible | eHostCoherent);

    auto ptr = staging.d.mapMemory(staging.mem, 0, data.size());
    std::memcpy(ptr, data.data(), data.size());
    staging.d.unmapMemory(staging.mem);

    auto cmdBuffers = vk.getCommands(1);
    vk::CommandBufferBeginInfo info{};
    vkassert(cmdBuffers[0].begin(&info));
    cmdBuffers[0].copyBuffer(staging.buffer, buffer,
                             vk::BufferCopy{0, 0, data.size()});
    cmdBuffers[0].end();
    vk.queues.mem().submit(std::array{vk::SubmitInfo{
        .commandBufferCount = 1, .pCommandBuffers = cmdBuffers.data()}});
  }
};

struct UBOBuffer {
  Buffer buffer;
  void *mapped = nullptr;
  size_t size = 0;
  UBOBuffer() = delete;
  UBOBuffer(Context &c, size_t size)
      : buffer(c, size, vk::BufferUsageFlagBits::eUniformBuffer,
               vk::MemoryPropertyFlagBits::eHostCoherent |
                   vk::MemoryPropertyFlagBits::eHostVisible) {
    mapped = buffer.d.mapMemory(buffer.mem, 0, size);
  }
  UBOBuffer(UBOBuffer &&other)
      : buffer(std::move(other.buffer)), mapped(other.mapped),
        size(other.size) {}
  ~UBOBuffer() {
    if (buffer.d)
      buffer.d.unmapMemory(buffer.mem);
  }
  template <typename T> void write(const T &data) {
    std::memcpy(mapped, reinterpret_cast<const void *>(&data), sizeof(data));
  }
};

const std::vector<Vertex> vertices = {{{-0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}},
                                      {{0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}},
                                      {{0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}},
                                      {{-0.5f, 0.5f}, {1.0f, 1.0f, 1.0f}}};
const std::vector<uint16_t> indices = {0, 1, 2, 2, 3, 0};

template <typename T>
concept contiguous = std::ranges::contiguous_range<T>;
template <typename T>
concept not_contiguous = not
std::ranges::contiguous_range<T>;

template <contiguous T> std::span<const uint8_t> bin(T in) {
  return {reinterpret_cast<const uint8_t *>(in.data()),
          in.size() * sizeof(in[0])};
}
template <typename T> std::span<const uint8_t> bin_view(T in) {
  return {reinterpret_cast<const uint8_t *>(&in), sizeof(in)};
}

bool processInput(UniformBuffer &buffer) {
  SDL_Event event;
  SDL_PollEvent(&event);
  if (event.type == SDL_KEYDOWN)
    switch (event.key.keysym.scancode) {
    default:
      break;
    }
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
            Buffer &vert, Buffer &ind, vk::DescriptorSet descriptor) {
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
    buffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, *c.layout, 0,
                              descriptor, {});
    buffer.bindVertexBuffers(0, std::array{vert.buffer},
                             std::array{vk::DeviceSize(0)});
    buffer.bindIndexBuffer(ind.buffer, 0, vk::IndexType::eUint16);
    setScissorViewport(c, buffer);
    buffer.drawIndexed(indices.size(), 1, 0, 0, 0);
    buffer.endRenderPass();
    buffer.end();
  }
}

void draw(Context &c, std::span<vk::CommandBuffer> buffers, Buffer &vert,
          Buffer &ind, std::span<vk::DescriptorSet> descriptors) {
  vkassert(
      c.device.waitForFences(std::array{*c.inflight_fen}, true, UINT64_MAX));
  c.device.resetFences(std::array{*c.inflight_fen});

  auto [result, imageIndex] =
      c.swapchain.acquireNextImage(UINT64_MAX, *c.image_available_sem);
  c.pool.reset();
  render(c, buffers, imageIndex, vert, ind,
         descriptors[imageIndex % descriptors.size()]);

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
  auto vert = Buffer(vk, vertices.size() * sizeof(vertices[0]),
                     eVertexBuffer | eTransferDst, eDeviceLocal);
  vert.write(vk, bin(vertices));

  return vert;
}

Buffer createIndBuffer(Context &vk) {
  using enum vk::BufferUsageFlagBits;
  using enum vk::MemoryPropertyFlagBits;
  auto vert = Buffer(vk, indices.size() * sizeof(indices[0]),
                     eIndexBuffer | eTransferDst, eDeviceLocal);
  vert.write(vk, bin(indices));

  return vert;
}

std::vector<UBOBuffer> createUBOs(Context &vk, int count) {
  std::vector<UBOBuffer> result;
  result.reserve(count);
  for (int i = 0; i != count; i++) {
    result.emplace_back(vk, sizeof(UniformBuffer));
  }
  return result;
}

std::vector<vk::DescriptorSet> createDescs(Context &vk, unsigned count,
                                           std::span<UBOBuffer> ubos) {
  auto descs = vk.getDescriptors(count);
  for (size_t i = 0; i < 2; i++) {
    vk::DescriptorBufferInfo buffer_info{.buffer = ubos[i].buffer.buffer,
                                         .offset = 0,
                                         .range = sizeof(UniformBuffer)};
    vk.device.updateDescriptorSets(
        {vk::WriteDescriptorSet{.dstSet = descs[i],
                                .dstBinding = 0,
                                .dstArrayElement = 0,
                                .descriptorCount = 1,
                                .descriptorType =
                                    vk::DescriptorType::eUniformBuffer,
                                .pBufferInfo = &buffer_info}},
        {});
  }
  return descs;
}

} // namespace

int main() {
  auto vk = Context(Window("triangles!", {.width = 600, .height = 800}));
  auto cmdBuffers = vk.getCommands(1);
  auto vert = createVertBuffer(vk);
  auto ind = createIndBuffer(vk);
  auto ubo = UniformBuffer();
  auto ubo_bufs = createUBOs(vk, 2);
  auto descs = createDescs(vk, 2, ubo_bufs);

  vk.queues.mem().waitIdle();
  int curr = 0;
  ubo.model = glm::rotate(glm::mat4(1.0f), glm::radians(90.f),
                          glm::vec3(0.0f, 0.0f, 1.0f));
  ubo.view =
      glm::lookAt(glm::vec3(2.0f, 2.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f),
                  glm::vec3(0.0f, 0.0f, 1.0f));
  ubo.proj = glm::perspective(glm::radians(45.0f),
                              vk.swapchain_extent.width /
                                  (float)vk.swapchain_extent.height,
                              0.1f, 10.0f);
  ubo.proj[1][1] *= -1;
  ubo_bufs[curr].write(ubo);
  auto start = std::chrono::high_resolution_clock::now();
  while (!processInput(ubo)) {
    auto now = std::chrono::high_resolution_clock::now();
    auto delta =
        std::chrono::duration<float, std::chrono::seconds::period>(now - start);
    ubo.model = glm::rotate(glm::mat4(1.0f), delta.count() * glm::radians(90.f),
                            glm::vec3(0.0f, 0.0f, 1.0f));
    ubo_bufs[curr].write(ubo);
    draw(vk, cmdBuffers, vert, ind, descs);
    curr++;
    if (curr >= 2)
      curr = 0;
  }
  vk.device.waitIdle();
  return 0;
}