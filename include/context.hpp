#pragma once
#include "queues.hpp"
#include "win_setup.hpp"
#include <vulkan/vulkan_raii.hpp>

struct Indicies {
  int graphics = -1, transfer = -1, present = -1;
  bool isComplete() const noexcept {
    return graphics != -1 || transfer != -1 || present != -1;
  }
};

struct Context {
  explicit Context(Window &&);
  ~Context();

  void recreateSwapchain();

  Window window;

  vk::raii::Context context;
  vk::raii::Instance instance;
  vk::raii::DebugUtilsMessengerEXT debug_messager;
  vk::raii::SurfaceKHR surface;
  vk::raii::Device device;
  Queues queues;
  vk::raii::SwapchainKHR swapchain;
  std::vector<vk::raii::ImageView> views;

  vk::Extent2D swapchain_extent;
  vk::Format format;
  Indicies indicies;
  vk::PhysicalDevice phys;
};

struct Renderer {
  Renderer(Context &);
  void recreateFramebuffers(Context &);
  ~Renderer();

  std::vector<vk::CommandBuffer>
  getCommands(uint32_t number,
              vk::CommandBufferLevel level = vk::CommandBufferLevel::ePrimary);
  std::vector<vk::DescriptorSet> getDescriptors(uint32_t number);

  vk::Device device;
  Queues queues;
  vk::raii::RenderPass pass;
  std::vector<vk::raii::Framebuffer> framebuffers;
  vk::raii::DescriptorSetLayout descriptor_layout;
  vk::raii::PipelineLayout layout;
  vk::raii::Pipeline pipeline;
  vk::raii::CommandPool pool;
  vk::raii::DescriptorPool desc_pool;

  vk::raii::Semaphore image_available_sem, render_done_sem;
  vk::raii::Fence inflight_fen;

  vk::Extent2D swapchain_extent;
};