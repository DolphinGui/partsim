#pragma once

#include <vector>

namespace vk::raii {
struct Framebuffer;
}

struct Renderer {
  std::vector<vk::raii::Framebuffer> framebuffer;
};