#pragma once

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <vulkan/vulkan.hpp>

struct Vertex {
  glm::vec2 pos;
  glm::vec3 color;

  static constexpr vk::VertexInputBindingDescription
  getBindingDescription() noexcept {
    return vk::VertexInputBindingDescription{.binding = 0,
                                             .stride = sizeof(Vertex),
                                             .inputRate =
                                                 vk::VertexInputRate::eVertex};
  }

  static constexpr std::array<vk::VertexInputAttributeDescription, 2>
  getAttributeDescriptions() noexcept {
    std::array<vk::VertexInputAttributeDescription, 2> attributeDescriptions{
        vk::VertexInputAttributeDescription{.location = 0,
                                            .binding = 0,
                                            .format = vk::Format::eR32G32Sfloat,
                                            .offset = offsetof(Vertex, pos)},
        vk::VertexInputAttributeDescription{.location = 1,
                                            .binding = 0,
                                            .format =
                                                vk::Format::eR32G32B32Sfloat,
                                            .offset = offsetof(Vertex, color)}};
    return attributeDescriptions;
  }
};