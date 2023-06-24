#include "glfwsetup.hpp"
#include <stdexcept>

inline size_t width = 800, height = 600;
inline void framebuffer_size_callback(GLFWwindow *window, int w, int h) {
  width = w;
  height = h;
}

GLFWwin setup_window() {
  glfwInit();
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
  GLFWwin window = {
      glfwCreateWindow(800, 600, "LearnOpenGL", nullptr, nullptr)};
  if (window.handle == nullptr) {
    glfwTerminate();
    throw std::runtime_error("Failed to create GLFW window");
  }
  glfwMakeContextCurrent(window.handle);

  glfwSetFramebufferSizeCallback(window.handle, framebuffer_size_callback);
  return window;
}