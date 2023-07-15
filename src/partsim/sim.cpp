#include <cmath>
#include <cstdlib>
#include <fmt/core.h>
#include <ranges>

#include "sim.hpp"
#include "ubo.hpp"

namespace partsim {
namespace {
glm::vec2 gen_s() {
  return {200.0 * std::rand() / float(RAND_MAX) - 100.0,
          200.0 * std::rand() / float(RAND_MAX) - 100.0};
}
glm::vec2 gen_v() {
  return {10.0 * std::rand() / float(RAND_MAX),
          10.0 * std::rand() / float(RAND_MAX)};
}
} // namespace
WorldState::WorldState() {
  std::srand(std::time(nullptr));
  locations.reserve(1000);
  velocities.reserve(1000);
  for (int i = 0; i != 4000; i++) {
    locations.push_back(gen_s());
    velocities.push_back(gen_v());
  }
  max_x = 100;
  min_x = -100;
  max_y = 100;
  min_y = -100;
}

namespace {
inline void bounds_check(glm::vec2 &s, glm::vec2 &v, float radius, float max_x,
                         float min_x, float max_y, float min_y) {
  if (s.x + radius > max_x) {
    s.x -= s.x - max_x + radius;
    v.x *= -1;
  } else if (s.x < min_x + radius) {
    s.x -= s.x - min_x - radius;
    v.x *= -1;
  }
  if (s.y + radius > max_y) {
    s.y -= s.y - max_y + radius;
    v.y *= -1;
  } else if (s.y < min_y + radius) {
    s.y -= s.y - min_y - radius;
    v.y *= -1;
  }
}

inline auto sqr(auto a) { return a * a; }

inline void collision_check(glm::vec2 &a_s, glm::vec2 &a_v, glm::vec2 &b_s,
                            glm::vec2 &b_v, float radius) {
  auto distance = sqr(a_s.x - b_s.x) + sqr(a_s.y - b_s.y);
  if (distance < sqr(2 * radius)) {
    auto d_s = a_s - b_s;
    auto diff_a = glm::dot(a_v - b_v, d_s) * (d_s) / glm::dot(d_s, d_s);
    auto diff_b = glm::dot(b_v - a_v, -d_s) * (-d_s) / glm::dot(d_s, d_s);
    a_v -= diff_a;
    b_v -= diff_b;
  }
}
} // namespace

void WorldState::process() {
  auto v_it = velocities.begin();
  for (auto &l : locations) {
    auto &v = *v_it++;
    l += v * world_delta.count();
    bounds_check(l, v, radius, max_x, min_x, max_y, min_y);
  }

  for (int i = 0; i < locations.size() - 1; ++i) {
    for (int j = 1 + i; j < locations.size(); ++j) {
      collision_check(locations.at(i), velocities.at(i), locations.at(j),
                      velocities.at(j), radius);
    }
  }
}

void WorldState::write(void *dst) const {
  std::memcpy(dst, locations.data(),
              sizeof(*locations.data()) * locations.size());
}
} // namespace partsim