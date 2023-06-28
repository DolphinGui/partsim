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
#include <vulkan/vulkan_raii.hpp>
#include <vulkan/vulkan_structs.hpp>

#include "build/frag.hpp"
#include "build/vert.hpp"
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

inline std::vector<const char *> getExtentions() {
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

  auto extensions = getExtentions();

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

SwapChainSupportDetails querySwapchainSupport(vk::SurfaceKHR surface,
                                              vk::PhysicalDevice device) {
  SwapChainSupportDetails details;
  details.capabilities = device.getSurfaceCapabilitiesKHR(surface);
  details.formats = device.getSurfaceFormatsKHR(surface);
  details.presentModes = device.getSurfacePresentModesKHR(surface);
  return details;
}

bool checkExtensionSupport(vk::PhysicalDevice device) {
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
  bool extensions_supported = checkExtensionSupport(device);
  auto swapchain_details = querySwapchainSupport(surface, device);
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

vk::Format setupSwapchain(Context &c, vk::PhysicalDevice physicalDevice) {
  SwapChainSupportDetails swapchain_support =
      querySwapchainSupport(*c.surface, physicalDevice);
  vk::SurfaceFormatKHR surface_format =
      chooseSwapSurfaceFormat(swapchain_support.formats);
  vk::PresentModeKHR present_mode =
      chooseSwapPresentMode(swapchain_support.presentModes);
  c.swapchain_extent = chooseSwapExtent(c, swapchain_support.capabilities);
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
      .imageExtent = c.swapchain_extent,
      .imageArrayLayers = 1,
      .imageUsage = vk::ImageUsageFlagBits::eColorAttachment,
      .imageSharingMode = vk::SharingMode::eExclusive,
      .preTransform = swapchain_support.capabilities.currentTransform,
      .compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque,
      .presentMode = present_mode,
      .clipped = true};

  auto sw = (*c.device).createSwapchainKHR(info);
  (*c.device).destroySwapchainKHR(sw);
  return surface_format.format;
}

void setupShaderAndPipeline(Context &c) {
  auto frag = vk::raii::ShaderModule(
      c.device,
      vk::ShaderModuleCreateInfo{.codeSize = build_shaders_frag_spv_len,
                                 .pCode = reinterpret_cast<const uint32_t *>(
                                     &build_shaders_frag_spv)});

  auto vert = vk::raii::ShaderModule(
      c.device,
      vk::ShaderModuleCreateInfo{.codeSize = build_shaders_vert_spv_len,
                                 .pCode = reinterpret_cast<const uint32_t *>(
                                     &build_shaders_vert_spv)});
  std::array shader_stages = {vk::PipelineShaderStageCreateInfo{
                                 .stage = vk::ShaderStageFlagBits::eVertex,
                                 .module = *vert,
                                 .pName = "main"},
                             vk::PipelineShaderStageCreateInfo{
                                 .stage = vk::ShaderStageFlagBits::eFragment,
                                 .module = *frag,
                                 .pName = "main"}};

  std::array dynamic_states = {vk::DynamicState::eViewport,
                               vk::DynamicState::eScissor};

  vk::PipelineVertexInputStateCreateInfo vert_in_info{};

  vk::PipelineInputAssemblyStateCreateInfo in_assembly{
      .topology = vk::PrimitiveTopology::eTriangleList};

  vk::PipelineViewportStateCreateInfo viewport_state{.viewportCount = 1,
                                                    .scissorCount = 1};

  vk::PipelineRasterizationStateCreateInfo rasterizer{
      .polygonMode = vk::PolygonMode::eFill,
      .cullMode = vk::CullModeFlagBits::eBack,
      .frontFace = vk::FrontFace::eClockwise,
      .lineWidth = 1.0};

  vk::PipelineMultisampleStateCreateInfo multisampling{
      .rasterizationSamples = vk::SampleCountFlagBits::e1};

  using enum vk::ColorComponentFlagBits;
  vk::PipelineColorBlendAttachmentState colorblend_attachment{
      .colorWriteMask = eR | eG | eB | eA};

  vk::PipelineColorBlendStateCreateInfo colorblending{
      .logicOp = vk::LogicOp::eCopy,
      .attachmentCount = 1,
      .pAttachments = &colorblend_attachment};

  std::array dyn_states = {vk::DynamicState::eViewport,
                              vk::DynamicState::eScissor};
  vk::PipelineDynamicStateCreateInfo dynamicState{
      .dynamicStateCount = dynamic_states.size(),
      .pDynamicStates = dynamic_states.data()};

  vk::PipelineLayoutCreateInfo pipeline_layout_info{};

  c.layout = c.device.createPipelineLayout(pipeline_layout_info);

  vk::GraphicsPipelineCreateInfo pipeline_info{
      .stageCount = 2,
      .pStages = shader_stages.data(),
      .pVertexInputState = &vert_in_info,
      .pInputAssemblyState = &in_assembly,
      .pViewportState = &viewport_state,
      .pRasterizationState = &rasterizer,
      .pMultisampleState = &multisampling,
      .pColorBlendState = &colorblending,
      .pDynamicState = &dynamicState,
      .layout = *c.layout,
      .renderPass = *c.pass,
      .subpass = 0};

  c.pipeline = c.device.createGraphicsPipeline(nullptr, pipeline_info);
}

void setupRenderpass(Context &c, vk::Format format) {
  using enum vk::SampleCountFlagBits;
  using enum vk::ImageLayout;
  using l = vk::AttachmentLoadOp;
  using s = vk::AttachmentStoreOp;

  auto color = vk::AttachmentDescription{.format = format,
                                         .samples = e1,
                                         .loadOp = l::eClear,
                                         .storeOp = s::eStore,
                                         .stencilLoadOp = l::eDontCare,
                                         .stencilStoreOp = s::eDontCare,
                                         .initialLayout = eUndefined,
                                         .finalLayout = ePresentSrcKHR};
  auto colorAttachmentRef = vk::AttachmentReference{
      .attachment = 0, .layout = vk::ImageLayout::eColorAttachmentOptimal};
  auto subpass = vk::SubpassDescription{
      .pipelineBindPoint = vk::PipelineBindPoint::eGraphics,
      .colorAttachmentCount = 1,
      .pColorAttachments = &colorAttachmentRef};
  auto pass_info = vk::RenderPassCreateInfo{.attachmentCount = 1,
                                            .pAttachments = &color,
                                            .subpassCount = 1,
                                            .pSubpasses = &subpass};
  c.pass = c.device.createRenderPass(pass_info);
}

} // namespace

Context::Context(GLFWwin &&win)
    : window(std::move(win)), context{}, instance(setupInstance(context)),
      debug_messager(setupDebug(instance)),
      surface(setupSurface(window, instance)), device(nullptr),
      swapchain(nullptr), pass(nullptr), layout(nullptr), pipeline{nullptr} {
  auto device = setupDevice(*this);
  auto format = setupSwapchain(*this, device);
  setupRenderpass(*this, format);
  setupShaderAndPipeline(*this);
}
Context setupVk(GLFWwin &&win) { return Context(std::move(win)); }