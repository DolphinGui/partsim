#pragma once
#include <GLFW/glfw3.h>

struct GLFWwin {
  GLFWwindow *handle{};
  GLFWwin() = default;
  GLFWwin(GLFWwindow *h) { handle = h; }
  ~GLFWwin() {
    if (handle)
      glfwDestroyWindow(handle);
  }
  GLFWwin(GLFWwin &&rhs) {
    handle = rhs.handle;
    rhs.handle = nullptr;
  }
  auto &operator=(GLFWwin &&rhs) {
    handle = rhs.handle;
    rhs.handle = nullptr;
    return *this;
  }
};

void setup_window();