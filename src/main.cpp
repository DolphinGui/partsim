#include <SDL2/SDL_events.h>
#include <cstddef>
#include <fmt/core.h>
#include <span>
#include <stdexcept>
#include <vector>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_structs.hpp>

#include "context.hpp"
#include "win_setup.hpp"

namespace {

void vkassert(vk::Result r) {
  if (r != vk::Result::eSuccess)
    throw std::runtime_error(vk::to_string(r));
}

bool processInput() {
  SDL_Event event;
  SDL_PollEvent(&event);
  return event.type == SDL_QUIT;
}

void setScissorViewport(Context &c, vk::CommandBuffer buffer) {
  vk::Viewport viewport{.x = 0,
                        .y = 0,
                        .width = static_cast<float>(c.swapchain_extent.width),
                        .height = static_cast<float>(c.swapchain_extent.height),
                        .minDepth = 0,
                        .maxDepth = 1.0};
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

void render(Context &c, std::span<vk::CommandBuffer> buffers, int index) {
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
    setScissorViewport(c, buffer);
    buffer.draw(3, 1, 0, 0);
    buffer.endRenderPass();
    buffer.end();
  }
}
float vertices[] = {-0.5f, -0.5f, 0.0f, 0.5f, -0.5f, 0.0f, 0.0f, 0.5f, 0.0f};

void draw(Context &c, std::span<vk::CommandBuffer> buffers) {
  vkassert(
      c.device.waitForFences(std::array{*c.inflight_fen}, true, UINT64_MAX));
  c.device.resetFences(std::array{*c.inflight_fen});

  auto [result, imageIndex] =
      c.swapchain.acquireNextImage(UINT64_MAX, *c.image_available_sem);
  c.pool.reset();
  render(c, buffers, imageIndex);

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
} // namespace

int main() {
  auto vk = Context(Window("triangles!", {.width = 600, .height = 800}));
  auto cmdBuffers = vk.getCommands(1);
  while (!processInput()) {
    draw(vk, cmdBuffers);
  }
  vk.device.waitIdle();
  return 0;
}