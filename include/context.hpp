#pragma once
#include "queues.hpp"
#include "win_setup.hpp"

struct Indicies {
  int graphics = -1, transfer = -1, present = -1;
  bool isComplete() const noexcept {
    return graphics != -1 || transfer != -1 || present != -1;
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
  vk::Pipeline pipeline;
  vk::CommandPool pool;
  vk::DescriptorPool desc_pool;

  std::array<vk::Semaphore, frames_in_flight> image_available_sem,
      render_done_sem;
  std::array<vk::Fence, frames_in_flight> inflight_fen;

  vk::Extent2D swapchain_extent;
};