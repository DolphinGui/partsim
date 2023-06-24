#define VULKAN_HPP_NO_CONSTRUCTORS
#include <GLFW/glfw3.h>
#include <cstddef>
#include <fmt/core.h>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>

#include "globals.hpp"
#include "glfwsetup.hpp"
#include "validation.hpp"

namespace {


void setup_instance() {

  if (enableValidation && !check_validation_support()) {
    throw std::runtime_error("validation layers requested, but not available!");
  }

  vk::ApplicationInfo appInfo{.pApplicationName = "Triangle",
                              .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
                              .pEngineName = "None",
                              .engineVersion = VK_MAKE_VERSION(1, 0, 0),
                              .apiVersion = VK_API_VERSION_1_0};

  uint32_t glfwExtensionCount = 0;
  const char **glfwExtensions;

  auto properties = context.enumerateInstanceExtensionProperties();

  glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

  vk::InstanceCreateInfo createInfo{
      .flags = {},
      .pApplicationInfo = &appInfo,
      .enabledLayerCount = validationLayers.size(),
      .ppEnabledLayerNames = validationLayers.data(),
      .enabledExtensionCount = glfwExtensionCount,
      .ppEnabledExtensionNames = glfwExtensions};

  instance = vk::raii::Instance(context, vk::createInstance(createInfo));
}

void setup_a() {}

void processInput(GLFWwindow *window) {
  if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
    glfwSetWindowShouldClose(window, true);
}
float vertices[] = {-0.5f, -0.5f, 0.0f, 0.5f, -0.5f, 0.0f, 0.0f, 0.5f, 0.0f};
} // namespace

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