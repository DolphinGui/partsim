#pragma once

#include <imgui/backends/imgui_impl_sdl2.h>
#include <imgui/backends/imgui_impl_vulkan.h>
#include <imgui/imgui.h>
#include <vulkan/vulkan.hpp>

struct Context;
struct Renderer;

struct GUI {
  vk::DescriptorPool imgui_pool;
  vk::Device device;

  GUI(Context &c, Renderer &vk);
  ~GUI();
  GUI(GUI &&other) : imgui_pool(other.imgui_pool), device(other.device) {
    other.imgui_pool = nullptr;
    other.device = nullptr;
  }
};