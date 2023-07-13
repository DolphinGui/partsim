#include "sim.hpp"
#include "ubo.hpp"
#include <ranges>

WorldState::WorldState() {
  locations.push_back(Vertex{{0, 0}});
  velocities.push_back({0.2, 0.2});
}

void WorldState::process() {
  auto v_it = velocities.begin();
  for (auto &l : locations) {
    auto &v = *v_it++;
    l.pos += v * delta.count();
    if (l.pos.x > max_x) {
      l.pos.x -= l.pos.x - max_x;
    } else if (l.pos.x < min_x) {
      l.pos.x -= l.pos.x - min_x;
    }
    if (l.pos.y > max_y) {
      l.pos.y -= l.pos.y - max_y;
    } else if (l.pos.y < min_y) {
      l.pos.y -= l.pos.y - min_y;
    }
  }
}