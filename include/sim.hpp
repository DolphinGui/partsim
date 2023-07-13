#pragma once

#include <chrono>
#include <glm/vec2.hpp>
#include <ratio>
#include <vector>

#include "vertex.hpp"

#define constant inline constexpr

namespace chron = std::chrono;
using ftime = chron::duration<float>;

constant float tick_rate = 50.0;
constant auto delta = ftime(chron::seconds(1)) / tick_rate;

struct WorldState {
  float max_x, min_x, max_y, min_y;
  std::vector<Vertex> locations;
  std::vector<glm::vec2> velocities;

  explicit WorldState();
  void process();
};

#undef constant