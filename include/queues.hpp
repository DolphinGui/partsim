#pragma once
#include <vulkan/vulkan_raii.hpp>

class Queues {
  vk::Queue graphics_q;
  vk::Queue transfer_q;

public:
  Queues() : graphics_q{nullptr}, transfer_q{nullptr} {}
  void set_graphics(vk::Queue &&rhs) { graphics_q = rhs; }
  void set_transfer(vk::Queue &&rhs) { transfer_q = rhs; }
  auto &render() { return graphics_q; }
  auto &mem() { return transfer_q; }
};