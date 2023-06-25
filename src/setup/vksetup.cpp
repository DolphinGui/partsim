#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>
#include <cstdio>
#include <fmt/core.h>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <utility>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_core.h>
#include <vulkan/vulkan_enums.hpp>
#include <vulkan/vulkan_handles.hpp>
#include <vulkan/vulkan_raii.hpp>
#include <vulkan/vulkan_structs.hpp>

#include "context.hpp"
#include "glfwsetup.hpp"
#include "queues.hpp"
#include "validation.hpp"
#include "vkformat.hpp"
#include "vksetup.hpp"

namespace {
struct Indicies {
  int graphics = -1, transfer = -1, present = -1;
  bool isComplete() const noexcept {
    return graphics != -1 || transfer != -1 || present != -1;
  }
};

struct SwapChainSupportDetails {
  vk::SurfaceCapabilitiesKHR capabilities;
  std::vector<vk::SurfaceFormatKHR> formats;
  std::vector<vk::PresentModeKHR> presentModes;
  bool ok() const noexcept { return !formats.empty() || !presentModes.empty(); }
};

constexpr std::array deviceExtensions = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};

inline VKAPI_ATTR VkBool32 VKAPI_CALL
debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT sev,
              VkDebugUtilsMessageTypeFlagsEXT type,
              const VkDebugUtilsMessengerCallbackDataEXT *data, void *user) {

  using enum vk::DebugUtilsMessageSeverityFlagBitsEXT;
  auto severity = vk::DebugUtilsMessageSeverityFlagBitsEXT(sev);
  if (severity & (eError | eWarning))
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

vk::raii::Instance setupInstance(vk::raii::Context &context) {

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

  return vk::raii::Instance(context, vk::createInstance(createInfo));
}

vk::raii::DebugUtilsMessengerEXT
setupDebug(const vk::raii::Instance &instance) {
  if (!enableValidation)
    return nullptr;
  using enum vk::DebugUtilsMessageSeverityFlagBitsEXT;
  using enum vk::DebugUtilsMessageTypeFlagBitsEXT;
  vk::DebugUtilsMessengerCreateInfoEXT createInfo{
      .messageSeverity = eError | eInfo | eVerbose | eWarning,
      .messageType =
          ePerformance | eDeviceAddressBinding | eGeneral | eValidation,
      .pfnUserCallback = debugCallback};

  vk::Result result = vk::Result(VK_ERROR_EXTENSION_NOT_PRESENT);
  VkDebugUtilsMessengerEXT messenger;

  if (auto func = (PFN_vkCreateDebugUtilsMessengerEXT)instance.getProcAddr(
          "vkCreateDebugUtilsMessengerEXT")) {
    result = vk::Result(
        func(*instance, (const VkDebugUtilsMessengerCreateInfoEXT *)&createInfo,
             nullptr, &messenger));
  }

  return vk::raii::DebugUtilsMessengerEXT(instance, messenger);
}

Indicies findQueueFamilies(vk::SurfaceKHR surface, vk::PhysicalDevice phys) {
  Indicies i;
  int index = 0;
  for (auto &property : phys.getQueueFamilyProperties()) {
    if (property.queueFlags & vk::QueueFlagBits::eGraphics &&
        i.graphics == -1) {
      i.graphics = index;
    }
    if (property.queueFlags & vk::QueueFlagBits::eTransfer &&
        i.transfer == -1) {
      i.transfer = index;
    }
    if (phys.getSurfaceSupportKHR(index, surface) && i.present == -1) {
      i.present = index;
    }

    if (i.graphics != -1 && i.transfer != -1 && i.present != -1)
      break;
    index++;
  }
  return i;
}

SwapChainSupportDetails querySwapChainSupport(vk::SurfaceKHR surface,
                                              vk::PhysicalDevice device) {
  SwapChainSupportDetails details;
  details.capabilities = device.getSurfaceCapabilitiesKHR(surface);
  details.formats = device.getSurfaceFormatsKHR(surface);
  details.presentModes = device.getSurfacePresentModesKHR(surface);
  return details;
}

bool checkDeviceExtensionSupport(vk::PhysicalDevice device) {
  auto availableExtensions = device.enumerateDeviceExtensionProperties();

  auto requiredExtensions = std::unordered_set<std::string>(
      deviceExtensions.begin(), deviceExtensions.end());

  for (const auto &extension : availableExtensions) {
    requiredExtensions.erase(extension.extensionName);
  }

  return requiredExtensions.empty();
}

vk::PresentModeKHR chooseSwapPresentMode(
    const std::vector<vk::PresentModeKHR> &availablePresentModes) {
  for (const auto &availablePresentMode : availablePresentModes) {
    if (availablePresentMode == vk::PresentModeKHR::eMailbox) {
      return availablePresentMode;
    }
  }

  return vk::PresentModeKHR::eFifo;
}

vk::SurfaceFormatKHR chooseSwapSurfaceFormat(
    const std::vector<vk::SurfaceFormatKHR> &availableFormats) {
  for (const auto &availableFormat : availableFormats) {
    if (availableFormat.format == vk::Format::eB8G8R8A8Srgb &&
        availableFormat.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear) {
      return availableFormat;
    }
  }

  return availableFormats[0];
}

vk::Extent2D chooseSwapExtent(Context &c,
                              const vk::SurfaceCapabilitiesKHR &capabilities) {
  if (capabilities.currentExtent.width !=
      std::numeric_limits<uint32_t>::max()) {
    return capabilities.currentExtent;
  } else {
    int width, height;
    glfwGetFramebufferSize(c.window.handle, &width, &height);

    vk::Extent2D extent = {static_cast<uint32_t>(width),
                           static_cast<uint32_t>(height)};

    extent.width = std::clamp(extent.width, capabilities.minImageExtent.width,
                              capabilities.maxImageExtent.width);
    extent.height =
        std::clamp(extent.height, capabilities.minImageExtent.height,
                   capabilities.maxImageExtent.height);

    return extent;
  }
}

std::pair<bool, Indicies> isDeviceSuitable(vk::SurfaceKHR surface,
                                           vk::PhysicalDevice device) {
  Indicies indices = findQueueFamilies(surface, device);
  bool extensions_supported = checkDeviceExtensionSupport(device);
  auto swapchain_details = querySwapChainSupport(surface, device);
  return {indices.isComplete() && extensions_supported &&
              swapchain_details.ok(),
          indices};
}
vk::PhysicalDevice setupDevice(Context &c) {
  auto phys_devices = vk::raii::PhysicalDevices(c.instance);
  if (phys_devices.empty())
    throw std::runtime_error("Could not find a vulkan-compatable device!");
  const vk::raii::PhysicalDevice *chosen = nullptr;
  Indicies indicies;
  for (auto &ph : phys_devices) {
    if (auto [cond, i] = isDeviceSuitable(*c.surface, *ph); cond) {
      chosen = &ph;
      indicies = i;
      break;
    }
  }
  auto &[graphics, present, transfer] = indicies;

  if (!chosen)
    throw std::runtime_error("Could not find a vulkan-compatable device!");

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
      .enabledExtensionCount = deviceExtensions.size(),
      .ppEnabledExtensionNames = deviceExtensions.data(),
      .pEnabledFeatures = &deviceFeatures,
  };

  if (enableValidation) {
    createInfo.enabledLayerCount =
        static_cast<uint32_t>(validationLayers.size());
    createInfo.ppEnabledLayerNames = validationLayers.data();
  } else {
    createInfo.enabledLayerCount = 0;
  }

  c.device = vk::raii::Device(*chosen, createInfo);
  c.queues.set_graphics(c.device.getQueue(graphics, 0).release());
  c.queues.set_transfer(c.device.getQueue(transfer, 0).release());
  return **chosen;
}

vk::raii::SurfaceKHR setupSurface(GLFWwin &window,
                                  vk::raii::Instance &instance) {
  auto info = vk::WaylandSurfaceCreateInfoKHR{
      .display = glfwGetWaylandDisplay(),
      .surface = glfwGetWaylandWindow(window.handle)};
  return instance.createWaylandSurfaceKHR(info);
}

void setupSwapchain(Context &c, vk::PhysicalDevice physicalDevice) {
  SwapChainSupportDetails swapchain_support =
      querySwapChainSupport(*c.surface, physicalDevice);
  vk::SurfaceFormatKHR surface_format =
      chooseSwapSurfaceFormat(swapchain_support.formats);
  vk::PresentModeKHR present_mode =
      chooseSwapPresentMode(swapchain_support.presentModes);
  vk::Extent2D extent = chooseSwapExtent(c, swapchain_support.capabilities);
  uint32_t image_count = swapchain_support.capabilities.minImageCount + 1;
  if (swapchain_support.capabilities.maxImageCount > 0 &&
      image_count > swapchain_support.capabilities.maxImageCount) {
    image_count = swapchain_support.capabilities.maxImageCount;
  }

  auto info = vk::SwapchainCreateInfoKHR{
      .surface = *c.surface,
      .minImageCount = image_count,
      .imageFormat = surface_format.format,
      .imageColorSpace = surface_format.colorSpace,
      .imageExtent = extent,
      .imageArrayLayers = 1,
      .imageUsage = vk::ImageUsageFlagBits::eColorAttachment,
      .imageSharingMode = vk::SharingMode::eExclusive,
      .preTransform = swapchain_support.capabilities.currentTransform,
      .compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque,
      .presentMode = present_mode,
      .clipped = true};

  auto sw = (*c.device).createSwapchainKHR(info);
  (*c.device).destroySwapchainKHR(sw);
  fmt::print("hello {}\n", (void *)*c.surface);
}

} // namespace

Context::Context(GLFWwin &&win)
    : window(std::move(win)), context{}, instance(setupInstance(context)),
      debug_messager(setupDebug(instance)),
      surface(setupSurface(window, instance)), device(nullptr),
      swapchain(nullptr) {
  auto device = setupDevice(*this);
  setupSwapchain(*this, device);
}
Context setupVk(GLFWwin &&win) { return Context(std::move(win)); }