#pragma once

#include <imgui/backends/imgui_impl_sdl2.h>
#include <imgui/backends/imgui_impl_vulkan.h>
#include <imgui/imgui.h>
#include <vulkan/vulkan.hpp>

struct Context;
struct Renderer;

struct GUI {
  ImGui_ImplVulkanH_Window win;
  vk::Device device;
  GUI(Context &c, Renderer &vk);
  ~GUI();
};