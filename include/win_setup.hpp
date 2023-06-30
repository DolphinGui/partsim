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

  void swap(Window &other) {
    SDL_Window *tmp = other.handle;
    other.handle = handle;
    handle = tmp;
  }
  Window(Window &&rhs) { swap(rhs); }
  Window &operator=(Window &&rhs) {
    swap(rhs);
    return *this;
  }

  Extent getBufferSize();

  vk::SurfaceKHR getSurface(vk::Instance);
  std::vector<const char*> getVkExtentions();

  SDL_Window *handle = nullptr;
};
