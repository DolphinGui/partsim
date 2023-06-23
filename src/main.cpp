#include "resource.hpp"
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <cstddef>
#include <fmt/core.h>

namespace {
size_t width = 800, height = 600;
void framebuffer_size_callback(GLFWwindow *window, int w, int h) {
  width = w;
  height = h;
}
auto a = [](GLFWwindow *w) { glfwDestroyWindow(w); };
using glfwWin = resource<GLFWwindow *, decltype(a)>;
template <typename... Ts> auto makeWindow(Ts... args) {
  return glfwWin{.handle = glfwCreateWindow(args...), .d = a};
}

auto setup_window() {
  glfwInit();
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
  auto window = makeWindow(800, 600, "LearnOpenGL", nullptr, nullptr);
  if (window.handle == NULL) {
    fmt::print("Failed to create GLFW window");
    glfwTerminate();
    std::exit(-1);
  }
  glfwMakeContextCurrent(window.handle);

  glfwSetFramebufferSizeCallback(window.handle, framebuffer_size_callback);
  return window;
}

void processInput(GLFWwindow *window) {
  if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
    glfwSetWindowShouldClose(window, true);
}
float vertices[] = {-0.5f, -0.5f, 0.0f, 0.5f, -0.5f, 0.0f, 0.0f, 0.5f, 0.0f};
} // namespace

constexpr const char *vertexShaderSource =
#include "../shaders/shader.vert"
    ;

int main() {
  {
    auto window = setup_window();

    while (!glfwWindowShouldClose(window.handle)) {
      processInput(window.handle);

      glfwSwapBuffers(window.handle);
      glfwPollEvents();
    }
  }
  glfwTerminate();
  return 0;
}