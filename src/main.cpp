#include <GLFW/glfw3.h>
#include <cstddef>
#include <fmt/core.h>

#include "context.hpp"
#include "glfwsetup.hpp"
#include "renderer.hpp"
#include "vk.hpp"

namespace {

void processInput(GLFWwindow *window) {
  if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
    glfwSetWindowShouldClose(window, true);
}
float vertices[] = {-0.5f, -0.5f, 0.0f, 0.5f, -0.5f, 0.0f, 0.0f, 0.5f, 0.0f};
} // namespace

int main() {
  {
    auto vk = setupVk(setup_window());
    auto render = setupRenderer(vk);
    while (!glfwWindowShouldClose(vk.window.handle)) {
      processInput(vk.window.handle);

      glfwSwapBuffers(vk.window.handle);
      glfwPollEvents();
    }
  }
  glfwTerminate();
  return 0;
}