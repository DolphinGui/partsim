#pragma once

#include <chrono>
#include <glm/vec2.hpp>
#include <ratio>
#include <vector>

#include "vertex.hpp"

#define constant inline constexpr
namespace partsim {
namespace chron = std::chrono;
using ftime = chron::duration<float>;

constant float tick_rate = 30.0;
constant auto world_delta = ftime(chron::seconds(1)) / tick_rate;

struct WorldState {
  float max_x, min_x, max_y, min_y;
  static constant float radius = 1.0;
  std::vector<glm::vec2> locations;
  std::vector<glm::vec2> velocities;

  explicit WorldState();
  void process();
};

} // namespace partsim
#undef constant