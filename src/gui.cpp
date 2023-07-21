#include "gui.hpp"

#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_to_string.hpp>

#include "context.hpp"

namespace {
inline void vkassert(VkResult r) {
  auto result = vk::Result(r);
  if (result != vk::Result::eSuccess) {
    throw std::runtime_error(vk::to_string(result));
  }
}
} // namespace


GUI::GUI(Context &c, Renderer &vk) {
  VkDevice d;
  ImGui::CreateContext();
  ImGuiIO &io = ImGui::GetIO();
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
  io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
  ImGui::StyleColorsDark();

  ImGui_ImplSDL2_InitForVulkan(c.window.handle);
  ImGui_ImplVulkan_InitInfo init_info = {};
  init_info.Instance = c.instance;
  init_info.PhysicalDevice = c.phys;
  init_info.Device = c.device;
  init_info.QueueFamily = c.indicies.graphics;
  init_info.Queue = c.queues.render();
  init_info.PipelineCache = nullptr;
  init_info.DescriptorPool = vk.desc_pool;
  init_info.Subpass = 0;
  init_info.MinImageCount = frames_in_flight;
  init_info.ImageCount = c.views.size();
  init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
  init_info.Allocator = nullptr;
  init_info.CheckVkResultFn = vkassert;
  ImGui_ImplVulkan_Init(&init_info, vk.pass);
}