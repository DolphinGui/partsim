#pragma once
#include "glfwsetup.hpp"
#include "queues.hpp"
#include <vulkan/vulkan_raii.hpp>

struct Context {
  Context(GLFWwin &&);
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
};