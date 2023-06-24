#pragma once
#include <GLFW/glfw3.h>

struct GLFWwin {
  GLFWwindow *handle{};
  ~GLFWwin() {
    if (handle)
      glfwDestroyWindow(handle);
  }
};


GLFWwin setup_window();