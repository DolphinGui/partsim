#include "gui.hpp"

#include <array>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_enums.hpp>
#include <vulkan/vulkan_to_string.hpp>

#include "backends/imgui_impl_vulkan.h"
#include "context.hpp"
#include "util/scope_guard.hpp"
#include "util/vkassert.hpp"

GUI::GUI(Context &c, Renderer &vk) : device(c.device) {
  using enum vk::DescriptorType;
  auto pool_sizes =
      std::to_array<vk::DescriptorPoolSize>({{eSampler, 1000},
                                             {eCombinedImageSampler, 1000},
                                             {eSampledImage, 1000},
                                             {eStorageImage, 1000},
                                             {eUniformTexelBuffer, 1000},
                                             {eStorageTexelBuffer, 1000},
                                             {eUniformBuffer, 1000},
                                             {eStorageBuffer, 1000},
                                             {eUniformBufferDynamic, 1000},
                                             {eStorageBufferDynamic, 1000},
                                             {eInputAttachment, 1000}});

  imgui_pool = device.createDescriptorPool(
      {.flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
       .maxSets = 1000,
       .poolSizeCount = pool_sizes.size(),
       .pPoolSizes = pool_sizes.data()});

  ImGui::CreateContext();
  ImGuiIO &io = ImGui::GetIO();
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
  io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
  ImGui::StyleColorsDark();

  ImGui_ImplSDL2_InitForVulkan(c.window.handle);
  ImGui_ImplVulkan_InitInfo init_info = {};
  init_info.Instance = c.instance;
  init_info.PhysicalDevice = c.phys;
  init_info.Device = device;
  init_info.QueueFamily = c.indicies.graphics;
  init_info.Queue = c.queues.render();
  init_info.DescriptorPool = imgui_pool;
  init_info.MinImageCount = frames_in_flight;
  init_info.ImageCount = c.views.size();
  init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
  init_info.CheckVkResultFn = vkassert;
  ImGui_ImplVulkan_Init(&init_info, vk.pass);
  vk.execute_immediately(
      [](vk::CommandBuffer cmd) { ImGui_ImplVulkan_CreateFontsTexture(cmd); });
  ImGui_ImplVulkan_DestroyFontUploadObjects();
}
GUI::~GUI() {
  if (device) {
    device.resetDescriptorPool(imgui_pool);
    device.destroyDescriptorPool(imgui_pool);
    ImGui_ImplVulkan_Shutdown();
  }
}