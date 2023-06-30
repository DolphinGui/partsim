#pragma once
#include <SDL2/SDL_video.h>
#include <cstddef>
#include <memory>
#include <span>
#include <string_view>
#include <vulkan/vulkan.hpp>

struct Extent {
  std::size_t width, height;
};

struct Window {
  Window(std::string_view title, Extent size);
  ~Window();

  Window(Window &&rhs) {
    handle = rhs.handle;
    rhs.moved_from = true;
  }
  Window &operator=(Window &&rhs) {
    handle = rhs.handle;
    rhs.moved_from = true;
    return *this;
  }

  Extent getBufferSize();

  vk::SurfaceKHR getSurface(vk::Instance);
  std::vector<const char *> getVkExtentions();

  SDL_Window *handle = nullptr;
  bool moved_from = false;
};
