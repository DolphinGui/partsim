#pragma once
#include <SDL2/SDL.h>
#include <cstddef>
#include <memory>
#include <string_view>
#include <vulkan/vulkan.hpp>

struct Extent {
  std::size_t width, height;
};

struct Window {
  Window(std::string_view title, Extent size);
  ~Window();

  Window(Window &&rhs);
  Window &operator=(Window &&rhs);

  bool shouldClose();
  Extent getBufferSize();

  vk::SurfaceKHR getSurface(vk::Instance);
  SDL_Window *handle;
};
