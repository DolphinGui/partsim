#pragma once

#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>

struct UniformBuffer {
  glm::mat4 model;
  glm::mat4 view;
  glm::mat4 proj;
};