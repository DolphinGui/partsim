#include <SDL2/SDL_vulkan.h>
#include <cstddef>
#include <cstdio>
#include <fmt/core.h>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <utility>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_to_string.hpp>

#include "constants.hpp"
#include "context.hpp"
#include "queues.hpp"
#include "ubo.hpp"
#include "util/scope_guard.hpp"
#include "util/vkformat.hpp"
#include "validation.hpp"
#include "vertex.hpp"
#include "win_setup.hpp"

namespace {

struct SwapChainSupportDetails {
  vk::SurfaceCapabilitiesKHR capabilities;
  std::vector<vk::SurfaceFormatKHR> formats;
  std::vector<vk::PresentModeKHR> presentModes;
  bool ok() const noexcept { return !formats.empty() || !presentModes.empty(); }
};

constexpr std::array deviceExtensions = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME,
    VK_KHR_SHADER_NON_SEMANTIC_INFO_EXTENSION_NAME};

inline VKAPI_ATTR VkBool32 VKAPI_CALL
debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT sev,
              VkDebugUtilsMessageTypeFlagsEXT type,
              const VkDebugUtilsMessengerCallbackDataEXT *data, void *user) {

  using enum vk::DebugUtilsMessageSeverityFlagBitsEXT;
  auto severity = vk::DebugUtilsMessageSeverityFlagBitsEXT(sev);
  fmt::print(stderr, "[{}][{}] {}\n", severity,
             vk::DebugUtilsMessageTypeFlagBitsEXT(type), data->pMessage);

  return VK_FALSE;
}

vk::Instance setupInstance(Window &win) {

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

  return vk::createInstance(createInfo);
}

vk::DebugUtilsMessengerEXT setupDebug(vk::Instance instance) {
  if (!enableValidation)
    return nullptr;
  using enum vk::DebugUtilsMessageSeverityFlagBitsEXT;
  using enum vk::DebugUtilsMessageTypeFlagBitsEXT;
  vk::DebugUtilsMessengerCreateInfoEXT createInfo{
      .messageSeverity = eError | eWarning,
      .messageType =
          ePerformance | eDeviceAddressBinding | eGeneral | eValidation,
      .pfnUserCallback = debugCallback};

  vk::Result result = vk::Result(VK_ERROR_EXTENSION_NOT_PRESENT);
  VkDebugUtilsMessengerEXT messenger;

  if (auto func = (PFN_vkCreateDebugUtilsMessengerEXT)instance.getProcAddr(
          "vkCreateDebugUtilsMessengerEXT")) {
    result = vk::Result(
        func(instance, (const VkDebugUtilsMessengerCreateInfoEXT *)&createInfo,
             nullptr, &messenger));
  }

  if (result != vk::Result::eSuccess)
    throw std::runtime_error(vk::to_string(result));

  return messenger;
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
    if (property.queueFlags & vk::QueueFlagBits::eCompute && i.compute == -1) {
      i.compute = index;
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

std::pair<int, Indicies> isDeviceSuitable(vk::SurfaceKHR surface,
                                          vk::PhysicalDevice device) {
  Indicies indices = findQueueFamilies(surface, device);
  bool extensions_supported = checkExtensionSupport(device);
  auto swapchain_details = querySwapchainSupport(surface, device);
  int score = 1;
  if (!(indices.isComplete() && extensions_supported && swapchain_details.ok()))
    return {-1, indices};
  auto props = device.getProperties();
  if (props.deviceType == vk::PhysicalDeviceType::eDiscreteGpu) {
    score += 10;
  }
  return {score, indices};
}

void setupDevice(Context &c) {
  auto phys_devices = c.instance.enumeratePhysicalDevices();
  if (phys_devices.empty())
    throw std::runtime_error("Could not find a vulkan-compatable device!");
  vk::PhysicalDevice chosen = nullptr;
  int chosen_score = 0;
  Indicies indicies;
  for (auto &ph : phys_devices) {
    if (auto [score, i] = isDeviceSuitable(c.surface, ph);
        score > chosen_score) {
      chosen = ph;
      indicies = i;
      chosen_score = score;
      break;
    }
  }
  c.indicies = indicies;
  auto &[graphics, present, transfer, comp] = indicies;

  if (!chosen)
    throw std::runtime_error("Could not find a vulkan-compatable device!");

  if (present != graphics)
    throw std::runtime_error(
        "Seperate graphics and present queue not supported");
  c.phys = chosen;

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

  c.device = chosen.createDevice(createInfo);
  c.queues.set_graphics(c.device.getQueue(graphics, 0));
  c.queues.set_transfer(c.device.getQueue(transfer, 0));
}

vk::Format setupSwapchain(Context &c) {
  SwapChainSupportDetails swapchain_support =
      querySwapchainSupport(c.surface, c.phys);
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
      .surface = c.surface,
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

void setupShaderAndPipeline(Context &c, Renderer &r) {
  auto f = shaders::fragment();
  auto frag = c.device.createShaderModule(vk::ShaderModuleCreateInfo{
      .codeSize = f.size_bytes(), .pCode = f.data()});
  auto frag_guard = ScopeGuard([&]() { c.device.destroyShaderModule(frag); });
  auto v = shaders::vertex();
  auto vert = c.device.createShaderModule(vk::ShaderModuleCreateInfo{
      .codeSize = v.size_bytes(), .pCode = v.data()});
  auto vert_guard = ScopeGuard([&]() { c.device.destroyShaderModule(vert); });

  ScreenScale scale = {1 / 50.0, 1 / 30.0};
  unsigned count = world::object_count;
  std::array spec_map{
      vk::SpecializationMapEntry{.constantID = 0,
                                 .offset = offsetof(ScreenScale, width),
                                 .size = sizeof(scale.width)},
      vk::SpecializationMapEntry{.constantID = 1,
                                 .offset = offsetof(ScreenScale, height),
                                 .size = sizeof(scale.height)},
      vk::SpecializationMapEntry{
          .constantID = 2, .offset = 0, .size = sizeof(count)}};

  vk::SpecializationInfo specialization_info{.mapEntryCount = spec_map.size(),
                                             .pMapEntries = spec_map.data(),
                                             .dataSize = sizeof(scale),
                                             .pData = &scale};

  std::array shader_stages = {vk::PipelineShaderStageCreateInfo{
                                  .stage = vk::ShaderStageFlagBits::eVertex,
                                  .module = vert,
                                  .pName = "main",
                                  .pSpecializationInfo = &specialization_info},
                              vk::PipelineShaderStageCreateInfo{
                                  .stage = vk::ShaderStageFlagBits::eFragment,
                                  .module = frag,
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

  auto bindings = std::to_array<vk::DescriptorSetLayoutBinding>(
      {{.binding = 0,
        .descriptorType = vk::DescriptorType::eStorageBuffer,
        .descriptorCount = 1,
        .stageFlags = vk::ShaderStageFlagBits::eVertex |
                      vk::ShaderStageFlagBits::eCompute},
       {.binding = 1,
        .descriptorType = vk::DescriptorType::eStorageBuffer,
        .descriptorCount = 1,
        .stageFlags = vk::ShaderStageFlagBits::eVertex |
                      vk::ShaderStageFlagBits::eCompute}});

  r.descriptor_layout = c.device.createDescriptorSetLayout(
      {.bindingCount = bindings.size(), .pBindings = bindings.data()});

  vk::PushConstantRange push_constant;
  push_constant.offset = 0;
  push_constant.size = sizeof(PushConstants);
  push_constant.stageFlags = vk::ShaderStageFlagBits::eVertex;

  vk::PipelineLayoutCreateInfo pipeline_layout_info{
      .setLayoutCount = 1,
      .pSetLayouts = &r.descriptor_layout,
      .pushConstantRangeCount = 1,
      .pPushConstantRanges = &push_constant};

  r.layout = c.device.createPipelineLayout(pipeline_layout_info);

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
      .layout = r.layout,
      .renderPass = r.pass,
      .subpass = 0};
  if (auto &&[err, result] =
          c.device.createGraphicsPipeline(nullptr, pipeline_info);
      err == vk::Result::eSuccess) {
    r.graphics_pipe = result;
  } else {
    throw std::runtime_error(vk::to_string(err));
  }
}

void setupCompute(Context &c, Renderer &r) {
  auto shader = shaders::compute();
  auto comp = c.device.createShaderModule(vk::ShaderModuleCreateInfo{
      .codeSize = shader.size_bytes(), .pCode = shader.data()});
  auto guard = ScopeGuard([&]() { c.device.destroyShaderModule(comp); });

  auto spec_map = std::to_array<vk::SpecializationMapEntry>(
      {{.constantID = 0,
        .offset = offsetof(world::constants_t, obj_count),
        .size = sizeof(world::constants.obj_count)},
       {.constantID = 1,
        .offset = offsetof(world::constants_t, max_x),
        .size = sizeof(world::constants.max_x)},
       {.constantID = 2,
        .offset = offsetof(world::constants_t, max_y),
        .size = sizeof(world::constants.max_y)}});

  vk::SpecializationInfo specialization_info{.mapEntryCount = spec_map.size(),
                                             .pMapEntries = spec_map.data(),
                                             .dataSize =
                                                 sizeof(world::constants),
                                             .pData = &world::constants};

  auto bindings = std::to_array<vk::DescriptorSetLayoutBinding>(
      {{.binding = 0,
        .descriptorType = vk::DescriptorType::eStorageBuffer,
        .descriptorCount = 1,
        .stageFlags = vk::ShaderStageFlagBits::eVertex |
                      vk::ShaderStageFlagBits::eCompute},
       {.binding = 1,
        .descriptorType = vk::DescriptorType::eStorageBuffer,
        .descriptorCount = 1,
        .stageFlags = vk::ShaderStageFlagBits::eVertex |
                      vk::ShaderStageFlagBits::eCompute}});

  r.compute_desc_layout = r.device.createDescriptorSetLayout(
      {.bindingCount = bindings.size(), .pBindings = bindings.data()});

  r.compute_layout = r.device.createPipelineLayout(
      {.setLayoutCount = 1, .pSetLayouts = &r.compute_desc_layout});

  auto [result, pipeline] = r.device.createComputePipeline(
      nullptr, {.stage = {.stage = vk::ShaderStageFlagBits::eCompute,
                          .module = comp,
                          .pName = "main",
                          .pSpecializationInfo = &specialization_info},
                .layout = r.compute_layout});
  if (result != vk::Result::eSuccess) {
    throw std::runtime_error(vk::to_string(result));
  }
  r.compute_pipe = pipeline;
}

void setupRenderpass(Context &c, Renderer &r) {
  using enum vk::SampleCountFlagBits;
  using enum vk::ImageLayout;
  using l = vk::AttachmentLoadOp;
  using s = vk::AttachmentStoreOp;

  auto color = vk::AttachmentDescription{.format = c.format,
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
  auto deps = std::to_array<vk::SubpassDependency>(
      {{.srcSubpass = VK_SUBPASS_EXTERNAL,
        .dstSubpass = 0,
        .srcStageMask = eColorAttachmentOutput | eComputeShader,
        .dstStageMask = eColorAttachmentOutput,
        .srcAccessMask = vk::AccessFlagBits::eShaderWrite,
        .dstAccessMask = vk::AccessFlagBits::eColorAttachmentWrite}});

  auto pass_info = vk::RenderPassCreateInfo{.attachmentCount = 1,
                                            .pAttachments = &color,
                                            .subpassCount = 1,
                                            .pSubpasses = &subpass,
                                            .dependencyCount = deps.size(),
                                            .pDependencies = deps.data()};
  r.pass = c.device.createRenderPass(pass_info);
}

void setupViews(Context &c) {
  std::vector<vk::Image> images = c.device.getSwapchainImagesKHR(c.swapchain);
  c.views.reserve(images.size());
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
  }
}

void setupFramebuffers(Context &c, Renderer &r) {
  for (int i = 0; i != c.views.size(); i++) {
    r.framebuffers.push_back(c.device.createFramebuffer(
        vk::FramebufferCreateInfo{.renderPass = r.pass,
                                  .attachmentCount = 1,
                                  .pAttachments = &c.views[i],
                                  .width = c.swapchain_extent.width,
                                  .height = c.swapchain_extent.height,
                                  .layers = 1}));
  }
}

void setupPool(Context &c, Renderer &r) {
  r.cmd_pool = c.device.createCommandPool(
      {.flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
       .queueFamilyIndex = static_cast<uint32_t>(c.indicies.graphics)});
}

void setupDescPool(Context &c, Renderer &r) {
  auto sizes = std::to_array<vk::DescriptorPoolSize>(
      {{.type = vk::DescriptorType::eUniformBuffer, .descriptorCount = 20},
       {.type = vk::DescriptorType::eStorageBuffer, .descriptorCount = 20}});
  r.desc_pool = c.device.createDescriptorPool({.maxSets = 40,
                                               .poolSizeCount = sizes.size(),
                                               .pPoolSizes = sizes.data()});
}
} // namespace

Context::Context(Window &&win)
    : window(std::move(win)), instance(setupInstance(window)),
      debug_messager(setupDebug(instance)),
      surface(window.getSurface(instance)), device(nullptr),
      swapchain(nullptr) {
  setupDevice(*this);
  format = setupSwapchain(*this);
  setupViews(*this);
}
Context::~Context() {
  for (auto view : views) {
    device.destroyImageView(view);
  }
  device.destroySwapchainKHR(swapchain);
  device.destroy();
  instance.destroySurfaceKHR(surface);
  if (auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)instance.getProcAddr(
          "vkDestroyDebugUtilsMessengerEXT")) {
    func(instance, (VkDebugUtilsMessengerEXT)debug_messager, nullptr);
  }
  instance.destroy();
}

void Context::recreateSwapchain() {
  device.waitIdle();

  views.clear();
  setupSwapchain(*this);
  setupViews(*this);
}

Renderer::Renderer(Context &c)
    : device(c.device), queues(c.queues), swapchain_extent(c.swapchain_extent) {
  setupRenderpass(c, *this);
  setupFramebuffers(c, *this);
  setupCompute(c, *this);
  setupShaderAndPipeline(c, *this);
  setupPool(c, *this);
  setupDescPool(c, *this);
  for (auto &available : image_available_sem) {
    available = c.device.createSemaphore({});
  }
  for (auto &done : render_done_sem) {
    done = c.device.createSemaphore({});
  }
  for (auto &in_flight : inflight_fen) {
    in_flight =
        c.device.createFence({.flags = vk::FenceCreateFlagBits::eSignaled});
  }
}
Renderer::~Renderer() {
  for (auto sem : image_available_sem) {
    device.destroySemaphore(sem);
  }
  for (auto sem : render_done_sem) {
    device.destroySemaphore(sem);
  }
  for (auto fence : inflight_fen) {
    device.destroyFence(fence);
  }
  device.destroyDescriptorPool(desc_pool);
  device.destroyCommandPool(cmd_pool);
  device.destroyPipeline(graphics_pipe);
  device.destroyPipelineLayout(layout);
  device.destroyDescriptorSetLayout(descriptor_layout);
  device.destroyPipeline(compute_pipe);
  device.destroyPipelineLayout(compute_layout);
  device.destroyDescriptorSetLayout(compute_desc_layout);
  for (auto buffer : framebuffers) {
    device.destroyFramebuffer(buffer);
  }
  device.destroyRenderPass(pass);
}

void Renderer::recreateFramebuffers(Context &c) {
  framebuffers.clear();
  setupFramebuffers(c, *this);
  swapchain_extent = c.swapchain_extent;
}

std::vector<vk::CommandBuffer>
Renderer::getCommands(uint32_t number, vk::CommandBufferLevel level) {
  return device.allocateCommandBuffers(vk::CommandBufferAllocateInfo{
      .commandPool = cmd_pool, .level = level, .commandBufferCount = number});
}