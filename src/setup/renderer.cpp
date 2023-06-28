#include "renderer.hpp"
#include "context.hpp"
#include "vk.hpp"
#include <vector>

Renderer setupRenderer(Context &c) {
  Renderer r{};
  std::vector<vk::Image> images = c.swapchain.getImages();
  std::vector<vk::raii::ImageView> views;
  views.reserve(images.size());
  for (int i = 0; i != images.size(); i++) {
    views.push_back(c.device.createImageView(vk::ImageViewCreateInfo{
        .image = images[i],
        .viewType = vk::ImageViewType::e2D,
        .format = c.format,
        .subresourceRange = {.aspectMask = vk::ImageAspectFlagBits::eColor,
                             .baseMipLevel = 0,
                             .levelCount = 1,
                             .baseArrayLayer = 0,
                             .layerCount = 1}}));
  }
  r.framebuffer.reserve(images.size());
  return r;
}