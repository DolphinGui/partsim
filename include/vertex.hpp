#pragma once

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <vulkan/vulkan.hpp>

struct Vertex {
  glm::vec2 pos;

  static constexpr vk::VertexInputBindingDescription
  getBindingDescription() noexcept {
    return vk::VertexInputBindingDescription{.binding = 0,
                                             .stride = sizeof(Vertex),
                                             .inputRate =
                                                 vk::VertexInputRate::eVertex};
  }

  static constexpr auto getAttributeDescriptions() noexcept {
    std::array attributeDescriptions{
        vk::VertexInputAttributeDescription{.location = 0,
                                            .binding = 0,
                                            .format = vk::Format::eR32G32Sfloat,
                                            .offset = offsetof(Vertex, pos)}};
    return attributeDescriptions;
  }
};