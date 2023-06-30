#include <SDL2/SDL.h>
#include <SDL2/SDL_video.h>
#include <SDL2/SDL_vulkan.h>
#include <fmt/core.h>
#include <stdexcept>
#include <vulkan/vulkan.hpp>

#include "win_setup.hpp"

Window::Window(std::string_view title, Extent size) {
  SDL_Init(SDL_INIT_VIDEO);
  handle = SDL_CreateWindow(title.data(), SDL_WINDOWPOS_UNDEFINED,
                            SDL_WINDOWPOS_UNDEFINED, size.width, size.height,
                            SDL_WINDOW_VULKAN | SDL_WINDOW_ALLOW_HIGHDPI);
}

vk::SurfaceKHR Window::getSurface(vk::Instance instance) {
  VkSurfaceKHR surface;
  if (SDL_Vulkan_CreateSurface(handle, instance, &surface) == SDL_TRUE)
    return surface;
  throw std::runtime_error("Could not create SDL vulkan surface");
}

Window::~Window() {
  SDL_DestroyWindow(handle);
  SDL_Quit();
}

Extent Window::getBufferSize() {
  int width, height;
  SDL_Vulkan_GetDrawableSize(handle, &width, &height);
  return {static_cast<size_t>(width), static_cast<size_t>(height)};
}

std::vector<const char *> Window::getVkExtentions() {
  unsigned count;
  SDL_Vulkan_GetInstanceExtensions(handle, &count, nullptr);
  std::vector<const char *> results;
  results.resize(count);
  SDL_Vulkan_GetInstanceExtensions(handle, &count, results.data());
  return results;
}