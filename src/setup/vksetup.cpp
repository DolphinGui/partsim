#include <SDL2/SDL_vulkan.h>
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
#include <vulkan/vulkan_to_string.hpp>

#include "build/frag.hpp"
#include "build/vert.hpp"
#include "context.hpp"
#include "queues.hpp"
#include "validation.hpp"
#include "vertex.hpp"
#include "vkformat.hpp"
#include "win_setup.hpp"

namespace {

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

vk::raii::Instance setupInstance(vk::raii::Context &context, Window &win) {

  if (enableValidation && !check_validation_support()) {
    throw std::runtime_error("validation layers requested, but not available!");
  }

  vk::ApplicationInfo appInfo{.pApplicationName = "Triangle",
                              .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
                              .pEngineName = "None",
                              .engineVersion = VK_MAKE_VERSION(1, 0, 0),
                              .apiVersion = VK_API_VERSION_1_0};

  auto extensions = win.getVkExtentions();
  extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

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

  if (result != vk::Result::eSuccess)
    throw std::runtime_error(vk::to_string(result));

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
    auto [width, height] = c.window.getBufferSize();

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
  c.indicies = indicies;
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

vk::raii::SurfaceKHR setupSurface(Window &window,
                                  vk::raii::Instance &instance) {
  return {instance, window.getSurface(*instance)};
}

vk::Format setupSwapchain(Context &c, vk::PhysicalDevice physicalDevice) {
  SwapChainSupportDetails swapchain_support =
      querySwapchainSupport(*c.surface, physicalDevice);
  vk::SurfaceFormatKHR surface_format =
      chooseSwapSurfaceFormat(swapchain_support.formats);
  c.format = surface_format.format;
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
  c.swapchain = c.device.createSwapchainKHR(info);
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

  vk::PipelineVertexInputStateCreateInfo vert_in_info{};
  auto bindingDescription = Vertex::getBindingDescription();
  auto attributeDescriptions = Vertex::getAttributeDescriptions();

  vert_in_info.vertexBindingDescriptionCount = 1;
  vert_in_info.vertexAttributeDescriptionCount =
      static_cast<uint32_t>(attributeDescriptions.size());
  vert_in_info.pVertexBindingDescriptions = &bindingDescription;
  vert_in_info.pVertexAttributeDescriptions = attributeDescriptions.data();

  vk::PipelineInputAssemblyStateCreateInfo in_assembly{
      .topology = vk::PrimitiveTopology::eTriangleList};

  vk::PipelineViewportStateCreateInfo viewport_state{.viewportCount = 1,
                                                     .scissorCount = 1};

  vk::PipelineRasterizationStateCreateInfo rasterizer{
      .polygonMode = vk::PolygonMode::eFill,
      .cullMode = vk::CullModeFlagBits::eNone,
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

  std::array dynamic_states = {vk::DynamicState::eViewport,
                               vk::DynamicState::eScissor};
  vk::PipelineDynamicStateCreateInfo dynamicState{
      .dynamicStateCount = dynamic_states.size(),
      .pDynamicStates = dynamic_states.data()};

  vk::DescriptorSetLayoutBinding uboLayoutBinding{
      .binding = 0,
      .descriptorType = vk::DescriptorType::eUniformBuffer,
      .descriptorCount = 1,
      .stageFlags = vk::ShaderStageFlagBits::eVertex};

  c.descriptor_layout = c.device.createDescriptorSetLayout(
      {.bindingCount = 1, .pBindings = &uboLayoutBinding});

  vk::PipelineLayoutCreateInfo pipeline_layout_info{
      .setLayoutCount = 1, .pSetLayouts = &*c.descriptor_layout};

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
      .attachment = 0, .layout = eColorAttachmentOptimal};
  auto subpass = vk::SubpassDescription{
      .pipelineBindPoint = vk::PipelineBindPoint::eGraphics,
      .colorAttachmentCount = 1,
      .pColorAttachments = &colorAttachmentRef};
  using enum vk::PipelineStageFlagBits;
  vk::SubpassDependency dependency{
      .srcSubpass = VK_SUBPASS_EXTERNAL,
      .dstSubpass = 0,
      .srcStageMask = eColorAttachmentOutput,
      .dstStageMask = eColorAttachmentOutput,
      .srcAccessMask = {},
      .dstAccessMask = vk::AccessFlagBits::eColorAttachmentWrite};

  auto pass_info = vk::RenderPassCreateInfo{.attachmentCount = 1,
                                            .pAttachments = &color,
                                            .subpassCount = 1,
                                            .pSubpasses = &subpass,
                                            .dependencyCount = 1,
                                            .pDependencies = &dependency};
  c.pass = c.device.createRenderPass(pass_info);
}
void setupViews(Context &c) {
  std::vector<vk::Image> images = c.swapchain.getImages();
  c.views.reserve(images.size());
  c.framebuffers.reserve(images.size());
  for (int i = 0; i != images.size(); i++) {
    c.views.push_back(c.device.createImageView(vk::ImageViewCreateInfo{
        .image = images[i],
        .viewType = vk::ImageViewType::e2D,
        .format = c.format,
        .subresourceRange = {.aspectMask = vk::ImageAspectFlagBits::eColor,
                             .baseMipLevel = 0,
                             .levelCount = 1,
                             .baseArrayLayer = 0,
                             .layerCount = 1}}));
    c.framebuffers.push_back(c.device.createFramebuffer(
        vk::FramebufferCreateInfo{.renderPass = *c.pass,
                                  .attachmentCount = 1,
                                  .pAttachments = &*c.views.back(),
                                  .width = c.swapchain_extent.width,
                                  .height = c.swapchain_extent.height,
                                  .layers = 1}));
  }
}

void setupPool(Context &c) {
  c.pool = c.device.createCommandPool(
      {.flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
       .queueFamilyIndex = static_cast<uint32_t>(c.indicies.graphics)});
}
void setupDescPool(Context &c) {
  vk::DescriptorPoolSize poolSize{.type = vk::DescriptorType::eUniformBuffer,
                                  .descriptorCount = 2};
  c.desc_pool = c.device.createDescriptorPool(
      {.maxSets = 2, .poolSizeCount = 1, .pPoolSizes = &poolSize});
}
} // namespace

Context::Context(Window &&win)
    : window(std::move(win)), context{},
      instance(setupInstance(context, window)),
      debug_messager(setupDebug(instance)),
      surface(setupSurface(window, instance)), device(nullptr),
      swapchain(nullptr), pass(nullptr), descriptor_layout(nullptr),
      layout(nullptr), pipeline{nullptr}, pool{nullptr}, desc_pool(nullptr),
      image_available_sem(nullptr), render_done_sem(nullptr),
      inflight_fen(nullptr) {
  phys = setupDevice(*this);
  format = setupSwapchain(*this, phys);
  setupRenderpass(*this, format);
  setupShaderAndPipeline(*this);
  setupViews(*this);
  setupPool(*this);
  setupDescPool(*this);
  image_available_sem = device.createSemaphore({});
  render_done_sem = device.createSemaphore({});
  inflight_fen =
      device.createFence({.flags = vk::FenceCreateFlagBits::eSignaled});
}

std::vector<vk::CommandBuffer>
Context::getCommands(uint32_t number, vk::CommandBufferLevel level) {
  return (*device).allocateCommandBuffers(vk::CommandBufferAllocateInfo{
      .commandPool = *pool, .level = level, .commandBufferCount = number});
}

std::vector<vk::DescriptorSet> Context::getDescriptors(uint32_t number) {
  std::vector<vk::DescriptorSetLayout> layouts(number, *descriptor_layout);
  return (*device).allocateDescriptorSets(
      vk::DescriptorSetAllocateInfo{.descriptorPool = *desc_pool,
                                    .descriptorSetCount = number,
                                    .pSetLayouts = layouts.data()});
}
Context::~Context() {}