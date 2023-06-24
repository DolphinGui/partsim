#pragma once
#include <vulkan/vulkan_raii.hpp>

class Queues {
  vk::raii::Queue graphics_q;
  vk::raii::Queue transfer_q;

public:
  Queues() : graphics_q{nullptr}, transfer_q{nullptr} {}
  void set_graphics(vk::raii::Queue &&rhs) { graphics_q.swap(rhs); }
  void set_transfer(vk::raii::Queue &&rhs) { transfer_q.swap(rhs); }
  auto &render() { return graphics_q; }
  auto &mem() { return transfer_q; }
};