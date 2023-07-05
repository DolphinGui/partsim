#include <SDL2/SDL_events.h>
#include <SDL2/SDL_video.h>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <fmt/core.h>
#include <fmt/ranges.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <numeric>
#include <ranges>
#include <span>
#include <stdexcept>
#include <vector>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_core.h>
#include <vulkan/vulkan_enums.hpp>
#include <vulkan/vulkan_handles.hpp>
#include <vulkan/vulkan_raii.hpp>
#include <vulkan/vulkan_structs.hpp>
#include <vulkan/vulkan_to_string.hpp>

#include "context.hpp"
#include "ubo.hpp"
#include "vertex.hpp"
#include "win_setup.hpp"

namespace {
struct UpdateSwapchainException {};
uint32_t findMemoryType(vk::PhysicalDevice phys, uint32_t typeFilter,
                        vk::MemoryPropertyFlags properties) {
  auto memProperties = phys.getMemoryProperties();
  for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
    if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags &
                                    properties) == properties) {
      return i;
    }
  }

  throw std::runtime_error("failed to find suitable memory type!");
}

void vkassert(vk::Result r) {
  if (r != vk::Result::eSuccess && r != vk::Result::eSuboptimalKHR)
    throw std::runtime_error(vk::to_string(r));
}

struct Buffer {
  vk::Buffer buffer{};
  vk::DeviceMemory mem{};
  vk::Device d{};
  Buffer(vk::Device device, vk::PhysicalDevice phys, size_t size,
         vk::BufferUsageFlags usage, vk::MemoryPropertyFlags properties) {
    d = device;
    buffer = d.createBuffer({.size = size,
                             .usage = usage,
                             .sharingMode = vk::SharingMode::eExclusive});
    auto mem_reqs = d.getBufferMemoryRequirements(buffer);

    mem = d.allocateMemory({.allocationSize = mem_reqs.size,
                            .memoryTypeIndex = findMemoryType(
                                phys, mem_reqs.memoryTypeBits, properties)});
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

  void write(vk::Device device, vk::PhysicalDevice phys, Renderer &vk,
             std::span<const uint8_t> data) {

    using enum vk::BufferUsageFlagBits;
    using enum vk::MemoryPropertyFlagBits;
    auto staging = Buffer(device, phys, data.size(), eTransferSrc,
                          eHostVisible | eHostCoherent);

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
  UBOBuffer(vk::Device d, vk::PhysicalDevice phys, size_t size)
      : buffer(d, phys, size, vk::BufferUsageFlagBits::eUniformBuffer,
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

const std::vector<Vertex> vertices = {
    {{1, 0}, {.2, .2, .2}},
    {{0.9921147, 0.12533323}, {.2, .2, .2}},
    {{0.96858317, 0.24868989}, {.2, .2, .2}},
    {{0.9297765, 0.36812454}, {.2, .2, .2}},
    {{0.87630665, 0.48175368}, {.2, .2, .2}},
    {{0.809017, 0.58778524}, {.2, .2, .2}},
    {{0.7289686, 0.6845471}, {.2, .2, .2}},
    {{0.637424, 0.77051324}, {.2, .2, .2}},
    {{0.5358268, 0.8443279}, {.2, .2, .2}},
    {{0.42577928, 0.90482706}, {.2, .2, .2}},
    {{0.309017, 0.95105654}, {.2, .2, .2}},
    {{0.18738131, 0.9822872}, {.2, .2, .2}},
    {{0.06279052, 0.9980267}, {.2, .2, .2}},
    {{-0.06279052, 0.9980267}, {.2, .2, .2}},
    {{-0.18738131, 0.9822872}, {.2, .2, .2}},
    {{-0.309017, 0.95105654}, {.2, .2, .2}},
    {{-0.42577928, 0.90482706}, {.2, .2, .2}},
    {{-0.5358268, 0.8443279}, {.2, .2, .2}},
    {{-0.637424, 0.77051324}, {.2, .2, .2}},
    {{-0.7289686, 0.6845471}, {.2, .2, .2}},
    {{-0.809017, 0.58778524}, {.2, .2, .2}},
    {{-0.87630665, 0.48175368}, {.2, .2, .2}},
    {{-0.9297765, 0.36812454}, {.2, .2, .2}},
    {{-0.96858317, 0.24868989}, {.2, .2, .2}},
    {{-0.9921147, 0.12533323}, {.2, .2, .2}},
    {{-1, 0}, {.2, .2, .2}},
    {{-0.9921147, -0.12533323}, {.2, .2, .2}},
    {{-0.96858317, -0.24868989}, {.2, .2, .2}},
    {{-0.9297765, -0.36812454}, {.2, .2, .2}},
    {{-0.87630665, -0.48175368}, {.2, .2, .2}},
    {{-0.809017, -0.58778524}, {.2, .2, .2}},
    {{-0.7289686, -0.6845471}, {.2, .2, .2}},
    {{-0.637424, -0.77051324}, {.2, .2, .2}},
    {{-0.5358268, -0.8443279}, {.2, .2, .2}},
    {{-0.42577928, -0.90482706}, {.2, .2, .2}},
    {{-0.309017, -0.95105654}, {.2, .2, .2}},
    {{-0.18738131, -0.9822872}, {.2, .2, .2}},
    {{-0.06279052, -0.9980267}, {.2, .2, .2}},
    {{0.06279052, -0.9980267}, {.2, .2, .2}},
    {{0.18738131, -0.9822872}, {.2, .2, .2}},
    {{0.309017, -0.95105654}, {.2, .2, .2}},
    {{0.42577928, -0.90482706}, {.2, .2, .2}},
    {{0.5358268, -0.8443279}, {.2, .2, .2}},
    {{0.637424, -0.77051324}, {.2, .2, .2}},
    {{0.7289686, -0.6845471}, {.2, .2, .2}},
    {{0.809017, -0.58778524}, {.2, .2, .2}},
    {{0.87630665, -0.48175368}, {.2, .2, .2}},
    {{0.9297765, -0.36812454}, {.2, .2, .2}},
    {{0.96858317, -0.24868989}, {.2, .2, .2}},
    {{0.9921147, -0.12533323}, {.2, .2, .2}},
};
const std::vector<uint16_t> indices = {
    0, 1,  2,  0, 2,  3,  0, 3,  4,  0, 4,  5,  0, 5,  6,  0, 6,  7,  0, 7,  8,
    0, 8,  9,  0, 9,  10, 0, 10, 11, 0, 11, 12, 0, 12, 13, 0, 13, 14, 0, 14, 15,
    0, 15, 16, 0, 16, 17, 0, 17, 18, 0, 18, 19, 0, 19, 20, 0, 20, 21, 0, 21, 22,
    0, 22, 23, 0, 23, 24, 0, 24, 25, 0, 25, 26, 0, 26, 27, 0, 27, 28, 0, 28, 29,
    0, 29, 30, 0, 30, 31, 0, 31, 32, 0, 32, 33, 0, 33, 34, 0, 34, 35, 0, 35, 36,
    0, 36, 37, 0, 37, 38, 0, 38, 39, 0, 39, 40, 0, 40, 41, 0, 41, 42, 0, 42, 43,
    0, 43, 44, 0, 44, 45, 0, 45, 46, 0, 46, 47, 0, 47, 48, 0, 48, 49,
};

template <typename T>
concept contiguous = std::ranges::contiguous_range<T>;

template <contiguous T> std::span<const uint8_t> bin(T in) {
  return {reinterpret_cast<const uint8_t *>(in.data()),
          in.size() * sizeof(in[0])};
}
template <typename T> std::span<const uint8_t> bin_view(T in) {
  return {reinterpret_cast<const uint8_t *>(&in), sizeof(in)};
}

using FTimePeriod = std::chrono::duration<float, std::chrono::seconds::period>;
bool processInput(UniformBuffer &ubo, Window &w, FTimePeriod delta,
                  bool &resized) {
  SDL_Event event;
  SDL_PollEvent(&event);
  static bool left = false;
  switch (event.type) {

  case SDL_KEYDOWN: {
    switch (event.key.keysym.scancode) {
    case SDL_SCANCODE_A:
      left = true;
      break;
    default:
      break;
    }
    break;
  }
  case SDL_KEYUP: {
    switch (event.key.keysym.scancode) {
    case SDL_SCANCODE_A:
      left = false;
      break;
    default:
      break;
    }
    break;
  }
  case SDL_WINDOWEVENT: {
    if (event.window.event == SDL_WINDOWEVENT_RESIZED)
      resized = true;
    break;
  }
  }
  if (left)
    ubo.dir.x += 1.0 * delta.count();

  return event.type == SDL_QUIT ||
         (event.type == SDL_KEYDOWN &&
          event.key.keysym.scancode == SDL_SCANCODE_C &&
          event.key.keysym.mod & KMOD_CTRL);
}

void setScissorViewport(vk::Extent2D swapchain_extent,
                        vk::CommandBuffer buffer) {
  buffer.setViewport(
      0, std::array{
             vk::Viewport{.x = 0,
                          .y = 0,
                          .width = static_cast<float>(swapchain_extent.width),
                          .height = static_cast<float>(swapchain_extent.height),
                          .minDepth = 0,
                          .maxDepth = 1.0}});

  buffer.setScissor(
      0, std::array{vk::Rect2D{.offset = {0, 0}, .extent = swapchain_extent}});
}

void render(Renderer &vk, std::span<vk::CommandBuffer> buffers, int index,
            Buffer &vert, Buffer &ind, vk::DescriptorSet descriptor) {
  for (auto &buffer : buffers) {
    vk::CommandBufferBeginInfo info{};
    vkassert(buffer.begin(&info));
    vk::ClearValue clearColor = {.color = {std::array{0.0f, 0.0f, 0.0f, 1.0f}}};
    buffer.beginRenderPass({.renderPass = *vk.pass,
                            .framebuffer = *vk.framebuffers[index],
                            .renderArea = {{0, 0}, vk.swapchain_extent},
                            .clearValueCount = 1,
                            .pClearValues = &clearColor},
                           vk::SubpassContents::eInline);
    buffer.bindPipeline(vk::PipelineBindPoint::eGraphics, *vk.pipeline);
    buffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, *vk.layout, 0,
                              descriptor, {});
    buffer.bindVertexBuffers(0, std::array{vert.buffer},
                             std::array{vk::DeviceSize(0)});
    buffer.bindIndexBuffer(ind.buffer, 0, vk::IndexType::eUint16);
    setScissorViewport(vk.swapchain_extent, buffer);
    buffer.drawIndexed(indices.size(), 1, 0, 0, 0);
    buffer.endRenderPass();
    buffer.end();
  }
}

vk::Result swapchain_acquire_result = vk::Result::eSuccess;

void draw(Renderer &c, vk::SwapchainKHR swapchain,
          std::span<vk::CommandBuffer> buffers, Buffer &vert, Buffer &ind,
          std::span<vk::DescriptorSet> descriptors) {
  vkassert(
      c.device.waitForFences(std::array{*c.inflight_fen}, true, UINT64_MAX));

  auto [result, imageIndex] = c.device.acquireNextImageKHR(
      swapchain, UINT64_MAX, *c.image_available_sem);
  if (result == vk::Result::eErrorOutOfDateKHR)
    throw UpdateSwapchainException{};
  else if (swapchain_acquire_result != vk::Result::eSuccess &&
           swapchain_acquire_result != vk::Result::eSuboptimalKHR) {
    throw std::runtime_error(vk::to_string(swapchain_acquire_result));
  }
  c.device.resetFences(std::array{*c.inflight_fen});

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
  vk::SwapchainKHR swapChains[] = {swapchain};
  presentInfo.waitSemaphoreCount = 1;
  presentInfo.pWaitSemaphores = signalSemaphores;
  presentInfo.swapchainCount = 1;
  presentInfo.pSwapchains = swapChains;
  presentInfo.pImageIndices = &imageIndex;
  vkassert(c.queues.render().presentKHR(presentInfo));
}

Buffer createVertBuffer(Context &vk, Renderer &r) {
  using enum vk::BufferUsageFlagBits;
  using enum vk::MemoryPropertyFlagBits;
  auto vert = Buffer(*vk.device, vk.phys, vertices.size() * sizeof(vertices[0]),
                     eVertexBuffer | eTransferDst, eDeviceLocal);
  vert.write(*vk.device, vk.phys, r, bin(vertices));

  return vert;
}

Buffer createIndBuffer(Context &vk, Renderer &r) {
  using enum vk::BufferUsageFlagBits;
  using enum vk::MemoryPropertyFlagBits;
  auto vert = Buffer(*vk.device, vk.phys, indices.size() * sizeof(indices[0]),
                     eIndexBuffer | eTransferDst, eDeviceLocal);
  vert.write(*vk.device, vk.phys, r, bin(indices));

  return vert;
}

std::vector<UBOBuffer> createUBOs(Context &vk, int count) {
  std::vector<UBOBuffer> result;
  result.reserve(count);
  for (int i = 0; i != count; i++) {
    result.emplace_back(*vk.device, vk.phys, sizeof(UniformBuffer));
  }
  return result;
}

std::vector<vk::DescriptorSet> createDescs(Renderer &vk, unsigned count,
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
void updateSwapchain(Context &context, Renderer &vk) {
  context.recreateSwapchain();
  vk.recreateFramebuffers(context);
}
} // namespace

int main() {

  auto context = Context(Window("triangles!", {.width = 600, .height = 600}));
  auto vk = Renderer(context);
  auto cmdBuffers = vk.getCommands(1);
  auto vert = createVertBuffer(context, vk);
  auto ind = createIndBuffer(context, vk);
  auto ubo = UniformBuffer();
  auto ubo_bufs = createUBOs(context, 2);
  auto descs = createDescs(vk, 2, ubo_bufs);

  vk.queues.mem().waitIdle();
  int curr = 0;
  ubo.dir = {0.0, 0.0};
  ubo_bufs[curr].write(ubo);
  auto prev = std::chrono::high_resolution_clock::now();
  FTimePeriod delta{};
  bool resized = false;
  while (!processInput(ubo, context.window, delta, resized)) {
    auto now = std::chrono::high_resolution_clock::now();
    delta = now - prev;
    ubo_bufs[curr].write(ubo);
    try {
      draw(vk, *context.swapchain, cmdBuffers, vert, ind, descs);
    } catch (UpdateSwapchainException e) {
      resized = true;
    }
    if (resized) {
      updateSwapchain(context, vk);
      resized = false;
      int width, height;
      SDL_GetWindowSize(context.window.handle, &width, &height);
      while (width == 0 && height == 0) {
        processInput(ubo, context.window, delta, resized);
      }
    }

    curr++;
    if (curr >= 2)
      curr = 0;
    prev = now;
  }
  vk.device.waitIdle();
  return 0;
}