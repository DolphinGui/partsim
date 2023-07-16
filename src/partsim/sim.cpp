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
  return {6.0 * signrand() / float(RAND_MAX),
          6.0 * signrand() / float(RAND_MAX)};
}
int indexVec(glm::vec2 s) {
  return int(s.x / (WorldState::max_x / WorldState::sector_count_x)) +
         int(s.y / (WorldState::max_y / WorldState::sector_count_y)) *
             WorldState::sector_count_x;
}
} // namespace
WorldState::WorldState() {
  std::srand(std::time(nullptr));
  locations.resize(sector_count * 2);
  velocities.resize(sector_count * 2);
  counts.resize(sector_count * 2, 0);
  for (int i = 0; i != objects; i++) {
    auto s = gen_s(max_x, max_y);
    int index = indexVec(s);
    getS(index) = s;
    getV(index) = gen_v();
    counts.at(index)++;
  }
}

namespace {
inline void bounds_check(glm::vec2 &s, glm::vec2 &v) {
  if (s.x + WorldState::radius > WorldState::max_x) {
    s.x -= s.x - WorldState::max_x + WorldState::radius;
    v.x *= -1;
  } else if (s.x < WorldState::radius) {
    s.x -= s.x - WorldState::radius;
    v.x *= -1;
  }
  if (s.y + WorldState::radius > WorldState::max_y) {
    s.y -= s.y - WorldState::max_y + WorldState::radius;
    v.y *= -1;
  } else if (s.y < WorldState::radius) {
    s.y -= s.y - WorldState::radius;
    v.y *= -1;
  }
}

inline auto sqr(auto a) { return a * a; }

inline void collision_check(glm::vec2 &a_s, glm::vec2 &a_v, glm::vec2 &b_s,
                            glm::vec2 &b_v) {
  auto distance = sqr(a_s.x - b_s.x) + sqr(a_s.y - b_s.y);
  if (distance < sqr(2 * WorldState::radius)) {
    auto d_s = a_s - b_s;
    auto diff_a = glm::dot(a_v - b_v, d_s) * (d_s) / glm::dot(d_s, d_s);
    auto diff_b = glm::dot(b_v - a_v, -d_s) * (-d_s) / glm::dot(d_s, d_s);
    a_v -= diff_a;
    b_v -= diff_b;
  }
}

inline void sortSector(glm::vec2 s, glm::vec2 v,
                       std::span<WorldState::Sector> other_s,
                       std::span<WorldState::Sector> other_v, size_t &count) {
  int index = indexVec(s);
  other_s[index][count] = s;
  other_v[index][count] = v;
  count++;
}

} // namespace

void WorldState::process() {
  for (int sector = base; sector != sector_count + base; sector++) {
    for (int i = 0; i != counts[sector]; i++) {
      locations[sector][i] += velocities[sector][i];
    }
  }

  for (int sector = base; sector != sector_count + base; sector++) {
    for (int i = 0; i != counts[sector]; i++) {
      bounds_check(locations[sector][i], velocities[sector][i]);
    }
  }

  int other = base == 0 ? sector_count : 0;
  for (int sector = base; sector != sector_count + base; sector++) {
    for (int i = 0; i != counts[sector]; i++) {
      sortSector(locations[sector][i], velocities[sector][i],
                 std::span(locations).subspan(other),
                 std::span(velocities).subspan(other), counts[sector + other]);
    }
  }

  if (base == 0) {
    base = sector_count;
  } else {
    base = 0;
  }
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