#include <SDL2/SDL_events.h>
#include <SDL2/SDL_scancode.h>
#include <SDL2/SDL_video.h>
#include <array>
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

#include "buffer.hpp"
#include "constants.hpp"
#include "context.hpp"
#include "gui.hpp"
#include "imgui.h"
#include "ubo.hpp"
#include "util/vkassert.hpp"
#include "vertex.hpp"
#include "win_setup.hpp"

namespace {
struct WorldS {
  constexpr static auto size = 5000;
  glm::vec2 pos[size];
  glm::vec2 vel[size];
  glm::vec4 color[size];
};
struct UpdateSwapchainException {};

constexpr auto vertices = std::to_array<Vertex>({{{1, 0}},
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
                                                 {{0.9921147, -0.12533323}}});

constexpr auto indices = std::to_array<uint16_t>({
    0, 1,  2,  0, 2,  3,  0, 3,  4,  0, 4,  5,  0, 5,  6,  0, 6,  7,  0, 7,  8,
    0, 8,  9,  0, 9,  10, 0, 10, 11, 0, 11, 12, 0, 12, 13, 0, 13, 14, 0, 14, 15,
    0, 15, 16, 0, 16, 17, 0, 17, 18, 0, 18, 19, 0, 19, 20, 0, 20, 21, 0, 21, 22,
    0, 22, 23, 0, 23, 24, 0, 24, 25, 0, 25, 26, 0, 26, 27, 0, 27, 28, 0, 28, 29,
    0, 29, 30, 0, 30, 31, 0, 31, 32, 0, 32, 33, 0, 33, 34, 0, 34, 35, 0, 35, 36,
    0, 36, 37, 0, 37, 38, 0, 38, 39, 0, 39, 40, 0, 40, 41, 0, 41, 42, 0, 42, 43,
    0, 43, 44, 0, 44, 45, 0, 45, 46, 0, 46, 47, 0, 47, 48, 0, 48, 49,
});

template <typename T>
concept contiguous = std::ranges::contiguous_range<T>;

template <contiguous T> std::span<const uint8_t> bin(T in) {
  return {reinterpret_cast<const uint8_t *>(in.data()),
          in.size() * sizeof(in[0])};
}
template <typename T> std::span<const uint8_t> bin_view(T in) {
  return {reinterpret_cast<const uint8_t *>(&in), sizeof(in)};
}

struct Position {
  float x = -1, y = 1;
  float zoom = 1;
};

// technically this recalculates all of the binds every time
// a button is pressed, which is a little suboptimal
inline void processKey(bool press_down, SDL_Scancode s, char &x, char &y,
                       char &zoom) {
  static bool up{}, down{}, left{}, right{}, zoom_in{}, zoom_out{};
  switch (s) {
  case SDL_SCANCODE_A:
    left = press_down;
    break;
  case SDL_SCANCODE_D:
    right = press_down;
    break;
  case SDL_SCANCODE_W:
    up = press_down;
    break;
  case SDL_SCANCODE_S:
    down = press_down;
    break;
  case SDL_SCANCODE_MINUS:
    zoom_in = press_down;
    break;
  case SDL_SCANCODE_EQUALS:
    zoom_out = press_down;
  default:
    break;
  }
  x = 0, y = 0, zoom = 0;
  if (left)
    x--;
  if (right)
    x++;
  if (up)
    y++;
  if (down)
    y--;
  if (zoom_in)
    zoom++;
  if (zoom_out)
    zoom--;
}

bool processInput(Window &w, bool &resized, Position &p) {
  SDL_Event event;
  SDL_PollEvent(&event);
  ImGui_ImplSDL2_ProcessEvent(&event);
  // auto &io = ImGui::GetIO();
  // if (io.WantCaptureKeyboard)
  //   return false;
  static char x = 0, y = 0, zoom = 0;
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
    processKey(true, event.key.keysym.scancode, x, y, zoom);
    break;
  }
  case SDL_KEYUP: {
    processKey(false, event.key.keysym.scancode, x, y, zoom);
    break;
  }
  default:
    break;
  }
  p.zoom *= std::exp(world::delta.count() * zoom);
  p.x += 2.0 * world::delta.count() * x / p.zoom;
  p.y += 2.0 * world::delta.count() * y / p.zoom;
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
            Buffer &ind, vk::DescriptorSet world, int index_count,
            PushConstants &constants) {
  vk::ClearValue clearColor = {.color = {std::array{0.0f, 0.0f, 0.0f, 1.0f}}};
  buffer.beginRenderPass({.renderPass = vk.pass,
                          .framebuffer = vk.framebuffers[index],
                          .renderArea = {{0, 0}, vk.swapchain_extent},
                          .clearValueCount = 1,
                          .pClearValues = &clearColor},
                         vk::SubpassContents::eInline);
  buffer.bindPipeline(vk::PipelineBindPoint::eGraphics, vk.graphics_pipe);
  buffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, vk.layout, 0,
                            world, {});
  buffer.bindVertexBuffers(0, std::array{vert.buffer},
                           std::array{vk::DeviceSize(0)});
  buffer.bindIndexBuffer(ind.buffer, 0, vk::IndexType::eUint16);
  setScissorViewport(vk.swapchain_extent, buffer);
  buffer.pushConstants(vk.layout, vk::ShaderStageFlagBits::eVertex, 0,
                       vk::ArrayProxy<const PushConstants>(1, &constants));
  buffer.drawIndexed(indices.size(), index_count, 0, 0, 0);
  ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), buffer);
  buffer.endRenderPass();
}

vk::Result swapchain_acquire_result = vk::Result::eSuccess;

void draw(Renderer &c, vk::SwapchainKHR swapchain, vk::CommandBuffer buffer,
          Buffer &vert, Buffer &ind, int instance_count,
          PushConstants &constants, int index,
          std::span<vk::DescriptorSet> world_descs) {
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

  vk::CommandBufferBeginInfo info{};
  vkassert(buffer.begin(&info));
  buffer.bindPipeline(vk::PipelineBindPoint::eCompute, c.compute_pipe);
  buffer.bindDescriptorSets(vk::PipelineBindPoint::eCompute, c.compute_layout,
                            0, world_descs[imageIndex % world_descs.size()],
                            {});
  buffer.dispatch(world::object_count / 256 + 1, 1, 1);
  using enum vk::AccessFlagBits;
  auto barrier = vk::MemoryBarrier{.srcAccessMask = eMemoryWrite | eMemoryRead,
                                   .dstAccessMask = eMemoryRead};
  buffer.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader,
                         vk::PipelineStageFlagBits::eVertexShader, {}, barrier,
                         {}, {});

  render(c, buffer, imageIndex, vert, ind,
         world_descs[imageIndex % world_descs.size()], instance_count,
         constants);
  buffer.end();

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

std::vector<vk::DescriptorSet> createWorldDescs(Renderer &vk, unsigned count,
                                                Buffer &world) {
  auto descs = vk.getDescriptors(count, vk.compute_desc_layout);
  for (size_t i = 0; i < frames_in_flight; i++) {
    vk::DescriptorBufferInfo buffer_info{
        .buffer = world.buffer, .offset = 0, .range = sizeof(WorldS)};
    vk.device.updateDescriptorSets(
        {{.dstSet = descs[i],
          .dstBinding = 0,
          .dstArrayElement = 0,
          .descriptorCount = 1,
          .descriptorType = vk::DescriptorType::eStorageBuffer,
          .pBufferInfo = &buffer_info}},
        {});
  }
  return descs;
}

void updateSwapchain(Context &context, Renderer &vk) {
  context.recreateSwapchain();
  vk.recreateFramebuffers(context);
}

int signrand() { return std::rand() * (std::rand() % 2 ? 1 : -1); }
WorldS genWorld() {
  WorldS world;
  srand(std::time(nullptr));
  for (auto &s : std::span(world.pos).subspan(0, world::object_count)) {
    s = {world::max_x * std::rand() / float(RAND_MAX),
         world::max_y * std::rand() / float(RAND_MAX)};
  }
  world.pos[0] = {6.8, 9};
  world.pos[1] = {5, 5};
  for (auto &v : std::span(world.vel).subspan(0, world::object_count)) {
    v = {20 * world::delta.count() * signrand() / float(RAND_MAX),
         20 * world::delta.count() * signrand() / float(RAND_MAX)};
  }
  world.vel[0] = {0, -world::delta.count()};
  world.vel[1] = {0, world::delta.count()};
  for (auto &c : std::span(world.color).subspan(0, world::object_count)) {
    c = glm::vec4(0.2, 0.2, 0.2, 0.2);
  }
  return world;
}

} // namespace

int main() {
  using namespace world;
  auto context = Context(Window("triangles!", {.width = 1000, .height = 600}));
  auto vk = Renderer(context);
  auto gui = GUI(context, vk);
  auto cmd_buffers = vk.getCommands(frames_in_flight);
  auto vert = createVertBuffer(context, vk);
  auto ind = createIndBuffer(context, vk);
  using enum vk::BufferUsageFlagBits;
  auto world_buf = Buffer(context.device, context.phys, sizeof(WorldS),
                          eStorageBuffer | eTransferDst,
                          vk::MemoryPropertyFlagBits::eDeviceLocal);
  auto world_desc = createWorldDescs(vk, frames_in_flight, world_buf);
  auto beginning = genWorld();
  world_buf.write(context.device, context.phys, vk, bin_view(beginning));
  auto pos = Position();

  vk.queues.mem().waitIdle();
  int curr = 0;

  FTime total_time{};
  unsigned frames{};
  bool resized = false;
  auto prev = std::chrono::high_resolution_clock::now();
  int fps = 0;
  while (!processInput(context.window, resized, pos)) {
    auto transform = PushConstants{glm::translate(
        glm::scale(glm::mat4(1.0), glm::vec3(pos.zoom)), {-pos.x, pos.y, 0})};
    auto now = std::chrono::high_resolution_clock::now();

    FTime dt = now - prev;
    if (dt < delta) {
      std::this_thread::sleep_for(delta - dt);
      auto now = std::chrono::high_resolution_clock::now();
      dt = now - prev;
    }

    total_time += dt;
    try {
      ImGui_ImplVulkan_NewFrame();
      ImGui_ImplSDL2_NewFrame(context.window.handle);

      ImGui::NewFrame();
      ImGui::Text("fps: %i", fps);
      ImGui::Text("pos: (%f, %f)", pos.x, pos.y);
      ImGui::Text("scaling: %f", pos.zoom);
      ImGui::EndFrame();
      ImGui::Render();
      draw(vk, context.swapchain, cmd_buffers[curr], vert, ind, object_count,
           transform, curr, world_desc);
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
      fps = frames;
      frames = 0;
    }
  }
  vk.device.waitIdle();
  return 0;
}