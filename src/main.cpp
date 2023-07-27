#include <SDL2/SDL_events.h>
#include <SDL2/SDL_scancode.h>
#include <SDL2/SDL_video.h>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <fmt/core.h>
#include <fmt/ranges.h>
#include <glm/ext/matrix_transform.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <numeric>
#include <ranges>
#include <span>
#include <stdexcept>
#include <thread>
#include <vector>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_to_string.hpp>

#include "context.hpp"
#include "gui.hpp"
#include "imgui.h"
#include "sim.hpp"
#include "ubo.hpp"
#include "util/vkassert.hpp"
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
    vk.queues.mem().waitIdle();
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
    {{1, 0}},
    {{0.9921147, 0.12533323}},
    {{0.96858317, 0.24868989}},
    {{0.9297765, 0.36812454}},
    {{0.87630665, 0.48175368}},
    {{0.809017, 0.58778524}},
    {{0.7289686, 0.6845471}},
    {{0.637424, 0.77051324}},
    {{0.5358268, 0.8443279}},
    {{0.42577928, 0.90482706}},
    {{0.309017, 0.95105654}},
    {{0.18738131, 0.9822872}},
    {{0.06279052, 0.9980267}},
    {{-0.06279052, 0.9980267}},
    {{-0.18738131, 0.9822872}},
    {{-0.309017, 0.95105654}},
    {{-0.42577928, 0.90482706}},
    {{-0.5358268, 0.8443279}},
    {{-0.637424, 0.77051324}},
    {{-0.7289686, 0.6845471}},
    {{-0.809017, 0.58778524}},
    {{-0.87630665, 0.48175368}},
    {{-0.9297765, 0.36812454}},
    {{-0.96858317, 0.24868989}},
    {{-0.9921147, 0.12533323}},
    {{-1, 0}},
    {{-0.9921147, -0.12533323}},
    {{-0.96858317, -0.24868989}},
    {{-0.9297765, -0.36812454}},
    {{-0.87630665, -0.48175368}},
    {{-0.809017, -0.58778524}},
    {{-0.7289686, -0.6845471}},
    {{-0.637424, -0.77051324}},
    {{-0.5358268, -0.8443279}},
    {{-0.42577928, -0.90482706}},
    {{-0.309017, -0.95105654}},
    {{-0.18738131, -0.9822872}},
    {{-0.06279052, -0.9980267}},
    {{0.06279052, -0.9980267}},
    {{0.18738131, -0.9822872}},
    {{0.309017, -0.95105654}},
    {{0.42577928, -0.90482706}},
    {{0.5358268, -0.8443279}},
    {{0.637424, -0.77051324}},
    {{0.7289686, -0.6845471}},
    {{0.809017, -0.58778524}},
    {{0.87630665, -0.48175368}},
    {{0.9297765, -0.36812454}},
    {{0.96858317, -0.24868989}},
    {{0.9921147, -0.12533323}},
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

using FTime = std::chrono::duration<float, std::chrono::seconds::period>;

struct Position {
  float x, y;
};

inline void processKey(bool press_down, SDL_Scancode s, char &x, char &y) {
  static bool up{}, down{}, left{}, right{};
  switch (s) {
  case SDL_SCANCODE_A:
    if (press_down)
      left = true;
    else
      left = false;
    break;
  case SDL_SCANCODE_D:
    if (press_down)
      right = true;
    else
      right = false;
    break;
  case SDL_SCANCODE_W:
    if (press_down)
      up = true;
    else
      up = false;
    break;
  case SDL_SCANCODE_S:
    if (press_down)
      down = true;
    else
      down = false;
    break;
  default:
    break;
  }
  x = 0, y = 0;
  if (left)
    x--;
  if (right)
    x++;
  if (up)
    y++;
  if (down)
    y--;
}

bool processInput(Window &w, bool &resized, Position &p) {
  SDL_Event event;
  SDL_PollEvent(&event);
  ImGui_ImplSDL2_ProcessEvent(&event);
  static char x = 0, y = 0;
  switch (event.type) {
  case SDL_WINDOWEVENT: {
    if (event.window.event == SDL_WINDOWEVENT_RESIZED) {
      resized = true;
    } else if (event.window.event == SDL_WINDOWEVENT_CLOSE &&
               event.window.windowID == SDL_GetWindowID(w.handle)) {
      fmt::print("quit requested\n");
      return true;
    }
    break;
  }
  case SDL_KEYDOWN: {
    processKey(true, event.key.keysym.scancode, x, y);
    break;
  }
  case SDL_KEYUP: {
    processKey(false, event.key.keysym.scancode, x, y);
    break;
  }
  default:
    break;
  }
  p.x += 2.0 / 60.0 * x;
  p.y += 2.0 / 60.0 * y;
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

void render(Renderer &vk, vk::CommandBuffer buffer, int index, Buffer &vert,
            Buffer &ind, vk::DescriptorSet descriptor, int index_count,
            PushConstants &constants) {
  vk::CommandBufferBeginInfo info{};
  vkassert(buffer.begin(&info));
  vk::ClearValue clearColor = {.color = {std::array{0.0f, 0.0f, 0.0f, 1.0f}}};
  buffer.beginRenderPass({.renderPass = vk.pass,
                          .framebuffer = vk.framebuffers[index],
                          .renderArea = {{0, 0}, vk.swapchain_extent},
                          .clearValueCount = 1,
                          .pClearValues = &clearColor},
                         vk::SubpassContents::eInline);
  buffer.bindPipeline(vk::PipelineBindPoint::eGraphics, vk.pipeline);
  buffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, vk.layout, 0,
                            descriptor, {});
  buffer.bindVertexBuffers(0, std::array{vert.buffer},
                           std::array{vk::DeviceSize(0)});
  buffer.bindIndexBuffer(ind.buffer, 0, vk::IndexType::eUint16);
  setScissorViewport(vk.swapchain_extent, buffer);
  buffer.pushConstants(vk.layout, vk::ShaderStageFlagBits::eVertex, 0,
                       vk::ArrayProxy<const PushConstants>(1, &constants));
  buffer.drawIndexed(indices.size(), index_count, 0, 0, 0);
  ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), buffer);
  buffer.endRenderPass();
  buffer.end();
}

vk::Result swapchain_acquire_result = vk::Result::eSuccess;

void draw(Renderer &c, vk::SwapchainKHR swapchain, vk::CommandBuffer buffer,
          Buffer &vert, Buffer &ind, std::span<vk::DescriptorSet> descriptors,
          int instance_count, PushConstants &constants, int index) {
  vkassert(c.device.waitForFences(c.inflight_fen[index], true, UINT64_MAX));

  auto [result, imageIndex] = c.device.acquireNextImageKHR(
      swapchain, UINT64_MAX, c.image_available_sem[index]);
  if (result == vk::Result::eErrorOutOfDateKHR)
    throw UpdateSwapchainException{};
  else if (swapchain_acquire_result != vk::Result::eSuccess &&
           swapchain_acquire_result != vk::Result::eSuboptimalKHR) {
    throw std::runtime_error(vk::to_string(swapchain_acquire_result));
  }
  c.device.resetFences(c.inflight_fen[index]);

  buffer.reset();

  render(c, buffer, imageIndex, vert, ind,
         descriptors[imageIndex % descriptors.size()], instance_count,
         constants);

  vk::Semaphore waitSemaphores[] = {c.image_available_sem[index]};
  vk::PipelineStageFlags waitStages[] = {
      vk::PipelineStageFlagBits::eColorAttachmentOutput};
  vk::Semaphore signalSemaphores[] = {c.render_done_sem[index]};
  std::array submit = {vk::SubmitInfo{.waitSemaphoreCount = 1,
                                      .pWaitSemaphores = waitSemaphores,
                                      .pWaitDstStageMask = waitStages,
                                      .commandBufferCount = 1,
                                      .pCommandBuffers = &buffer,
                                      .signalSemaphoreCount = 1,
                                      .pSignalSemaphores = signalSemaphores}};

  c.queues.render().submit(submit, c.inflight_fen[index]);

  vk::SwapchainKHR swap_chains[] = {swapchain};
  vkassert(c.queues.render().presentKHR({.waitSemaphoreCount = 1,
                                         .pWaitSemaphores = signalSemaphores,
                                         .swapchainCount = 1,
                                         .pSwapchains = swap_chains,
                                         .pImageIndices = &imageIndex}));
}

Buffer createVertBuffer(Context &vk, Renderer &r) {
  using enum vk::BufferUsageFlagBits;
  using enum vk::MemoryPropertyFlagBits;
  auto vert = Buffer(vk.device, vk.phys, vertices.size() * sizeof(vertices[0]),
                     eVertexBuffer | eTransferDst, eDeviceLocal);
  vert.write(vk.device, vk.phys, r, bin(vertices));

  return vert;
}

Buffer createIndBuffer(Context &vk, Renderer &r) {
  using enum vk::BufferUsageFlagBits;
  using enum vk::MemoryPropertyFlagBits;
  auto vert = Buffer(vk.device, vk.phys, indices.size() * sizeof(indices[0]),
                     eIndexBuffer | eTransferDst, eDeviceLocal);
  vert.write(vk.device, vk.phys, r, bin(indices));

  return vert;
}

std::vector<UBOBuffer> createUBOs(Context &vk, int count) {
  std::vector<UBOBuffer> result;
  result.reserve(count);
  for (int i = 0; i != count; i++) {
    result.emplace_back(vk.device, vk.phys, sizeof(UniformBuffer));
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
  using World = partsim::WorldState;
  auto context = Context(Window("triangles!", {.width = 1000, .height = 600}));
  auto vk = Renderer(context);
  auto gui = GUI(context, vk);
  auto cmdBuffers = vk.getCommands(frames_in_flight);
  auto vert = createVertBuffer(context, vk);
  auto ind = createIndBuffer(context, vk);
  auto ubo_bufs = createUBOs(context, frames_in_flight);
  auto descs = createDescs(vk, frames_in_flight, ubo_bufs);
  auto world = World();
  auto pos = Position{.x = -1, .y = 1};

  vk.queues.mem().waitIdle();
  int curr = 0;
  world.process();
  world.write(ubo_bufs[curr].mapped);

  FTime total_time{};
  unsigned frames{};
  bool resized = false;
  auto prev = std::chrono::high_resolution_clock::now();
  int instances = world.objects;

  while (!processInput(context.window, resized, pos)) {
    auto transform =
        PushConstants{glm::translate(glm::mat4(1.0), {-pos.x, pos.y, 0})};
    auto now = std::chrono::high_resolution_clock::now();

    FTime delta = now - prev;
    if (delta < partsim::world_delta) {
      std::this_thread::sleep_for(partsim::world_delta - delta);
      auto now = std::chrono::high_resolution_clock::now();
      delta = now - prev;
    }
    world.process();
    world.write(ubo_bufs[curr].mapped);

    total_time += delta;
    try {
      ImGui_ImplVulkan_NewFrame();
      ImGui_ImplSDL2_NewFrame(context.window.handle);

      ImGui::NewFrame();
      ImGui::ShowDemoWindow();
      ImGui::EndFrame();
      ImGui::Render();
      draw(vk, context.swapchain, cmdBuffers[curr], vert, ind, descs, instances,
           transform, curr);
    } catch (UpdateSwapchainException e) {
      resized = true;
    }
    if (resized) {
      updateSwapchain(context, vk);
      resized = false;
      int width, height;
      SDL_GetWindowSize(context.window.handle, &width, &height);
      while (width == 0 && height == 0) {
        processInput(context.window, resized, pos);
      }
    }

    curr++;
    if (curr >= frames_in_flight)
      curr = 0;
    prev = now;
    frames++;
    if (total_time > FTime(1)) {
      total_time -= FTime(1);
      fmt::print("fps: {}\n", frames);
      frames = 0;
    }
  }
  vk.device.waitIdle();
  return 0;
}