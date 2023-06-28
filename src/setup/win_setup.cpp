#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>
// #include <SDL2/
#include <SDL2/SDL_video.h>
#include <stdexcept>
#include <vulkan/vulkan.hpp>

#include "win_setup.hpp"

Window::Window(std::string_view title, Extent size) {

  SDL_Init(SDL_INIT_VIDEO);
  handle = SDL_CreateWindow(title.data(), SDL_WINDOWPOS_UNDEFINED,
                            SDL_WINDOWPOS_UNDEFINED, size.width, size.height,
                            SDL_WINDOW_VULKAN | SDL_WINDOW_ALLOW_HIGHDPI);
}