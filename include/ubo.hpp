#pragma once

#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>

// this should be a combination of model view projection
struct PushConstants {
	glm::mat4 transform;
};