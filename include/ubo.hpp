#pragma once

#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>

struct UniformBuffer {
  glm::vec2 coord[1024];
};

// this should be a combination of model view projection
struct PushConstants {
	glm::mat4 transform;
};