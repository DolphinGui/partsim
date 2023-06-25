#pragma once
#include <vulkan/vulkan_raii.hpp>

// realistically I could maybe optimize by allowing for 1 queue
// but then I'd have to possibly sync access so instead it's simpler
// to let the driver sync by having seperate queues.
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