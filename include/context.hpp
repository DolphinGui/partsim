#pragma once
#include "glfwsetup.hpp"
#include "queues.hpp"
#include <vulkan/vulkan_enums.hpp>
#include <vulkan/vulkan_raii.hpp>

struct Indicies {
  int graphics = -1, transfer = -1, present = -1;
  bool isComplete() const noexcept {
    return graphics != -1 || transfer != -1 || present != -1;
  }
};

struct Context {
  Context(GLFWwin &&);

  std::vector<vk::raii::CommandBuffer>
  getCommands(uint32_t number,
              vk::CommandBufferLevel level = vk::CommandBufferLevel::ePrimary);

  GLFWwin window;

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
  vk::Extent2D swapchain_extent;
  vk::Format format;
  Indicies indicies;
  std::vector<vk::raii::ImageView> views;
  std::vector<vk::raii::Framebuffer> framebuffers;
  vk::raii::CommandPool pool;
};