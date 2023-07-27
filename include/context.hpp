#pragma once

#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_handles.hpp>

#include "queues.hpp"
#include "util/scope_guard.hpp"
#include "util/vkassert.hpp"
#include "win_setup.hpp"

struct Indicies {
  int graphics = -1, transfer = -1, present = -1, compute = -1;
  bool isComplete() const noexcept {
    return graphics != -1 || transfer != -1 || present != -1 || compute != -1;
  }
};

constexpr size_t frames_in_flight = 2;

struct Context {
  explicit Context(Window &&);
  ~Context();

  void recreateSwapchain();

  Window window;

  vk::Instance instance;
  vk::DebugUtilsMessengerEXT debug_messager;
  vk::SurfaceKHR surface;
  vk::Device device;
  Queues queues;
  vk::SwapchainKHR swapchain;
  std::vector<vk::ImageView> views;

  vk::Extent2D swapchain_extent;
  vk::Format format;
  Indicies indicies;
  vk::PhysicalDevice phys;
};

struct Renderer {
  Renderer(Context &);
  void recreateFramebuffers(Context &);
  void execute_immediately(auto &&F) {
    auto cmd = device.allocateCommandBuffers(
        {.commandPool = cmd_pool,
         .level = vk::CommandBufferLevel::ePrimary,
         .commandBufferCount = 1});
    auto guard =
        ScopeGuard([&]() { device.freeCommandBuffers(cmd_pool, cmd); });
    vk::CommandBufferBeginInfo info{};
    vkassert(cmd.front().begin(&info));
    F(cmd.front());
    cmd.front().end();
    queues.render().submit(vk::SubmitInfo{.commandBufferCount = 1,
                                          .pCommandBuffers = &cmd.front()});
    queues.render().waitIdle();
  }
  ~Renderer();

  std::vector<vk::CommandBuffer>
  getCommands(uint32_t number,
              vk::CommandBufferLevel level = vk::CommandBufferLevel::ePrimary);
  std::vector<vk::DescriptorSet> getDescriptors(uint32_t number);

  vk::Device device;
  Queues queues;
  vk::RenderPass pass;
  std::vector<vk::Framebuffer> framebuffers;
  vk::DescriptorSetLayout descriptor_layout;
  vk::PipelineLayout layout;
  vk::Pipeline graphics_pipe;
  vk::DescriptorSetLayout compute_desc_layout;
  vk::PipelineLayout compute_layout;
  vk::Pipeline compute_pipe;
  vk::CommandPool cmd_pool;
  vk::DescriptorPool desc_pool;

  std::array<vk::Semaphore, frames_in_flight> image_available_sem,
      render_done_sem;
  std::array<vk::Fence, frames_in_flight> inflight_fen;

  vk::Extent2D swapchain_extent;
};