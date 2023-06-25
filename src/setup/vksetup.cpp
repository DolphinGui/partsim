#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>
#include <cstdio>
#include <fmt/core.h>
#include <stdexcept>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>
#include <vulkan/vulkan_structs.hpp>

#include "glfwsetup.hpp"
#include "globals.hpp"
#include "queues.hpp"
#include "validation.hpp"
#include "vkformat.hpp"
#include "vksetup.hpp"

namespace {

constexpr std::array deviceExtensions = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};

inline VKAPI_ATTR VkBool32 VKAPI_CALL
debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT sev,
               VkDebugUtilsMessageTypeFlagsEXT type,
               const VkDebugUtilsMessengerCallbackDataEXT *data, void *user) {

  using enum vk::DebugUtilsMessageSeverityFlagBitsEXT;
  auto severity = vk::DebugUtilsMessageSeverityFlagBitsEXT(sev);
  if (severity & eError || severity & eWarning)
    fmt::print(stderr, "[{}][{}] {}\n", severity,
               vk::DebugUtilsMessageTypeFlagBitsEXT(type), data->pMessage);

  return VK_FALSE;
}

inline std::vector<const char *> getExtensions() {
  uint32_t glfwExtensionCount = 0;
  const char **glfwExtensions;
  glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

  std::vector<const char *> extensions(glfwExtensions,
                                       glfwExtensions + glfwExtensionCount);

  if (enableValidation) {
    extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
  }

  return extensions;
}

void setupInstance() {

  if (enableValidation && !check_validation_support()) {
    throw std::runtime_error("validation layers requested, but not available!");
  }

  vk::ApplicationInfo appInfo{.pApplicationName = "Triangle",
                              .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
                              .pEngineName = "None",
                              .engineVersion = VK_MAKE_VERSION(1, 0, 0),
                              .apiVersion = VK_API_VERSION_1_0};

  auto extensions = getExtensions();

  vk::InstanceCreateInfo createInfo{
      .flags = {},
      .pApplicationInfo = &appInfo,
      .enabledLayerCount = validationLayers.size(),
      .ppEnabledLayerNames = validationLayers.data(),
      .enabledExtensionCount = static_cast<uint32_t>(extensions.size()),
      .ppEnabledExtensionNames = extensions.data()};

  instance = vk::raii::Instance(context, vk::createInstance(createInfo));
}

inline vk::Result createDebugUtilsMessengerEXT(
    const VkDebugUtilsMessengerCreateInfoEXT &pCreateInfo) {
  vk::Result result = vk::Result(VK_ERROR_EXTENSION_NOT_PRESENT);
  VkDebugUtilsMessengerEXT messenger;

  if (auto func = (PFN_vkCreateDebugUtilsMessengerEXT)instance.getProcAddr(
          "vkCreateDebugUtilsMessengerEXT")) {
    result = vk::Result(func(*instance, &pCreateInfo, nullptr, &messenger));
  }
  debug_messager = vk::raii::DebugUtilsMessengerEXT(instance, messenger);

  return result;
}

void setupDebug() {
  if (!enableValidation)
    return;
  using enum vk::DebugUtilsMessageSeverityFlagBitsEXT;
  using enum vk::DebugUtilsMessageTypeFlagBitsEXT;
  vk::DebugUtilsMessengerCreateInfoEXT createInfo{
      .messageSeverity = eError | eInfo | eVerbose | eWarning,
      .messageType =
          ePerformance | eDeviceAddressBinding | eGeneral | eValidation,
      .pfnUserCallback = debugCallback};

  if (createDebugUtilsMessengerEXT(createInfo) != vk::Result::eSuccess) {
    throw std::runtime_error("failed to set up debug messenger!");
  }
}

bool isDeviceSuitable() {
  constexpr std::array deviceExtensions = {
      VK_KHR_SWAPCHAIN_EXTENSION_NAME};
  return false;
}

void setupDevice() {
  auto phys_devices = vk::raii::PhysicalDevices(instance);
  if (phys_devices.empty())
    throw std::runtime_error("Could not find a vulkan-compatable device!");
  // too lazy to score device, just pick first device
  int index = 0;
  int graphics = -1, mem = -1, present = -1;
  auto &phys = phys_devices.front();
  for (auto &property : phys.getQueueFamilyProperties()) {
    if (property.queueFlags & vk::QueueFlagBits::eGraphics && graphics == -1) {
      graphics = index;
    }
    if (property.queueFlags & vk::QueueFlagBits::eTransfer && mem == -1) {
      mem = index;
    }
    if (phys.getSurfaceSupportKHR(index, *surface) && present == -1) {
      present = index;
    }

    if (graphics != -1 && mem != -1 && present != -1)
      break;
    index++;
  }

  if (graphics == -1 || mem == -1 || present == -1)
    throw std::runtime_error("could not find a queue");
  if (present != graphics)
    throw std::runtime_error(
        "Seperate graphics and present queue not supported");

  float queuePriority = 1.0f;
  vk::DeviceQueueCreateInfo queueCreateInfo{.queueFamilyIndex =
                                                static_cast<uint32_t>(graphics),
                                            .queueCount = 1,
                                            .pQueuePriorities = &queuePriority};
  vk::PhysicalDeviceFeatures deviceFeatures{};
  vk::DeviceCreateInfo createInfo{
      .queueCreateInfoCount = 1,
      .pQueueCreateInfos = &queueCreateInfo,
      .pEnabledFeatures = &deviceFeatures,
  };

  if (enableValidation) {
    createInfo.enabledLayerCount =
        static_cast<uint32_t>(validationLayers.size());
    createInfo.ppEnabledLayerNames = validationLayers.data();
  } else {
    createInfo.enabledLayerCount = 0;
  }

  device = vk::raii::Device(phys, createInfo);
  queues.set_graphics(device.getQueue(graphics, 0));
  queues.set_transfer(device.getQueue(mem, 0));
}

void setupSurface() {
  auto info = vk::WaylandSurfaceCreateInfoKHR{
      .display = glfwGetWaylandDisplay(),
      .surface = glfwGetWaylandWindow(window.handle)};
  surface = instance.createWaylandSurfaceKHR(info);
}
} // namespace

void setupVk() {
  setupInstance();
  setupDebug();
  setupSurface();
  setupDevice();
}