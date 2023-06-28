#include <GLFW/glfw3.h>
#include <cstddef>
#include <fmt/core.h>
#include <span>
#include <vector>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_enums.hpp>
#include <vulkan/vulkan_raii.hpp>
#include <vulkan/vulkan_structs.hpp>

#include "context.hpp"
#include "glfwsetup.hpp"

namespace {

void processInput(GLFWwindow *window) {
  if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
    glfwSetWindowShouldClose(window, true);
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

void render(Context &c, std::span<vk::raii::CommandBuffer> buffers, int index) {
  for (auto &buffer : buffers) {
    buffer.begin({});

    vk::ClearValue clearColor = {.color = {std::array{0.0f, 0.0f, 0.0f, 1.0f}}};
    buffer.beginRenderPass({.renderPass = *c.pass,
                            .framebuffer = *c.framebuffers[index],
                            .renderArea = {{0, 0}, c.swapchain_extent},
                            .clearValueCount = 1,
                            .pClearValues = &clearColor},
                           vk::SubpassContents::eInline);
    setScissorViewport(c, *buffer);
    buffer.draw(3, 1, 0, 0);
    buffer.end();
  }
}
float vertices[] = {-0.5f, -0.5f, 0.0f, 0.5f, -0.5f, 0.0f, 0.0f, 0.5f, 0.0f};
} // namespace

int main() {
  {
    auto vk = Context(setup_window());
    auto cmdBuffers = vk.getCommands(1);
    while (!glfwWindowShouldClose(vk.window.handle)) {
      processInput(vk.window.handle);

      glfwSwapBuffers(vk.window.handle);
      glfwPollEvents();
    }
  }
  glfwTerminate();
  return 0;
}