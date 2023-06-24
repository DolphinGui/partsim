#include <GLFW/glfw3.h>
#include <cstddef>
#include <fmt/core.h>

#include "glfwsetup.hpp"
#include "globals.hpp"
#include "validation.hpp"
#include "vksetup.hpp"

namespace {

void processInput(GLFWwindow *window) {
  if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
    glfwSetWindowShouldClose(window, true);
}
float vertices[] = {-0.5f, -0.5f, 0.0f, 0.5f, -0.5f, 0.0f, 0.0f, 0.5f, 0.0f};
} // namespace

int main() {
  {
    setup_window();
    setup_vk();
    while (!glfwWindowShouldClose(window.handle)) {
      processInput(window.handle);

      glfwSwapBuffers(window.handle);
      glfwPollEvents();
    }
  }
  glfwTerminate();
  return 0;
}