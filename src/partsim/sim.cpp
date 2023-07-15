#include <cmath>
#include <cstdlib>
#include <fmt/core.h>
#include <fmt/ranges.h>
#include <ranges>
#include <vector>

#include "sim.hpp"
#include "ubo.hpp"

namespace partsim {
namespace {
glm::vec2 gen_s(float x, float y) {
  return {x * std::rand() / float(RAND_MAX), y * std::rand() / float(RAND_MAX)};
}

int signrand() { return std::rand() - RAND_MAX / 2; }

glm::vec2 gen_v() {
  return {3.0 * signrand() / float(RAND_MAX),
          3.0 * signrand() / float(RAND_MAX)};
}
} // namespace
WorldState::WorldState() {
  std::srand(std::time(nullptr));
  locations.resize(4);
  velocities.resize(4);
  counts.resize(4, 0);
  for (int i = 0; i != objects; i++) {
    auto s = gen_s(max_x, max_y);
    int index = int(s.x / (max_x / sector_count_x)) +
                int(s.y / (max_y / sector_count_y)) * sector_count_x;
    getS(index) = s;
    getV(index) = gen_v();
    counts.at(index)++;
  }
  fmt::print("[{}]\n", fmt::join(counts, ", "));
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
  for (int sector = 0; sector != sector_count; sector++) {
    for (int i = 0; i != counts[sector]; i++) {
      locations[sector][i] += velocities[sector][i];
    }
  }
  for (int sector = 0; sector != sector_count; sector++) {
    for (int i = 0; i != counts[sector]; i++) {
      bounds_check(locations[sector][i], velocities[sector][i], radius, max_x,
                   0, max_y, 0);
    }
  }

  // for (int i = 0; i < locations.size() - 1; ++i) {
  //   for (int j = 1 + i; j < locations.size(); ++j) {
  //     collision_check(locations.at(i), velocities.at(i), locations.at(j),
  //                     velocities.at(j), radius);
  //   }
  // }
}

void WorldState::write(void *dst) const {
  size_t offset = 0;
  for (int i = 0; i != sector_count; i++) {
    std::memcpy((char *)dst + offset, locations[i].data(),
                sizeof(*locations[i].data()) * counts[i]);
    offset += sizeof(*locations[i].data()) * counts[i];
  }
}
} // namespace partsim