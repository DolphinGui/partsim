#pragma once
#include "queues.hpp"
#include "win_setup.hpp"
#include <vulkan/vulkan_enums.hpp>
#include <vulkan/vulkan_raii.hpp>

struct Indicies {
  int graphics = -1, transfer = -1, present = -1;
  bool isComplete() const noexcept {
    return graphics != -1 || transfer != -1 || present != -1;
  }
};

struct Context {
  Context(Window &&);
  ~Context();

  std::vector<vk::CommandBuffer>
  getCommands(uint32_t number,
              vk::CommandBufferLevel level = vk::CommandBufferLevel::ePrimary);
  void recreateSwapchain();

  Window window;

  vk::raii::Context context;
  vk::raii::Instance instance;
  vk::raii::DebugUtilsMessengerEXT debug_messager;
  vk::raii::SurfaceKHR surface;
  vk::raii::Device device;
  Queues queues;
  vk::raii::SwapchainKHR swapchain;
  vk::raii::RenderPass pass;
  vk::raii::PipelineLayout layout;
  vk::raii::Pipeline pipeline;
  std::vector<vk::raii::ImageView> views;
  std::vector<vk::raii::Framebuffer> framebuffers;
  vk::raii::CommandPool pool;

  vk::raii::Semaphore image_available_sem, render_done_sem;
  vk::raii::Fence inflight_fen;

  vk::Extent2D swapchain_extent;
  vk::Format format;
  Indicies indicies;
  vk::PhysicalDevice phys;
};