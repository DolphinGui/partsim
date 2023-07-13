#include <fmt/core.h>
#include <ranges>

#include "sim.hpp"
#include "ubo.hpp"

namespace partsim {
WorldState::WorldState() {
  locations.push_back(Vertex{{0, 0}});
  velocities.push_back({5, 5});
  max_x = 50;
  min_x = -50;
  max_y = 30;
  min_y = -30;
}

namespace {
inline void bounds_check(glm::vec2 &l, glm::vec2 &v, float radius, float max_x,
                         float min_x, float max_y, float min_y) {
  if (l.x + radius > max_x) {
    l.x -= l.x - max_x + radius;
    v.x *= -1;
  } else if (l.x < min_x + radius) {
    l.x -= l.x - min_x - radius;
    v.x *= -1;
  }
  if (l.y + radius > max_y) {
    l.y -= l.y - max_y + radius;
    v.y *= -1;
  } else if (l.y < min_y + radius) {
    l.y -= l.y - min_y - radius;
    v.y *= -1;
  }
}
} // namespace

void WorldState::process() {
  auto v_it = velocities.begin();
  for (auto &l : locations) {
    auto &v = *v_it++;
    l.pos += v * world_delta.count();
    bounds_check(l.pos, v, radius, max_x, min_x, max_y, min_y);
  }
}
} // namespace partsim