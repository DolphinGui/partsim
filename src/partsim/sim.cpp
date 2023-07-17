#include <cmath>
#include <cstdlib>
#include <fmt/core.h>
#include <fmt/ranges.h>
#include <ranges>
#include <vector>

#include "sim.hpp"
#include "ubo.hpp"
#include "util/glm_format.hpp"
#include "util/zip.hpp"

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
int indexVec(glm::vec2 s) {
  int result = int(s.x / (WorldState::max_x / WorldState::sector_count_x)) +
               int(s.y / (WorldState::max_y / WorldState::sector_count_y)) *
                   WorldState::sector_count_x;
  if (result >= WorldState::sector_count)
    throw std::logic_error("sector is out of bounds");
  return result;
}
} // namespace
WorldState::WorldState() {
  std::srand(std::time(nullptr));
  locations_vec.resize(sector_count * 2, {});
  velocities_vec.resize(sector_count * 2, {});
  counts_vec.resize(sector_count * 2, 0);
  for (int _ = 0; _ != objects; _++) {
    auto gen = gen_s(max_x, max_y);
    int index = indexVec(gen);
    size_t &i = counts()[index];
    locations()[index][i] = gen;
    velocities()[index][i] = gen_v();
    i++;
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
                       std::span<WorldState::Sector> other_loc,
                       std::span<WorldState::Sector> other_vec,
                       std::span<size_t> other_count) {
  int index = indexVec(s);
  other_loc[index][other_count[index]] = s;
  other_vec[index][other_count[index]] = v;
  other_count[index]++;
}

} // namespace

void WorldState::process() {
  for (auto &&[loc, vel, c] : zip::Range(locations(), velocities(), counts())) {
    for (int i = 0; i != c; i++) {
      loc[i] += vel[i];
    }
  }
  for (auto &&[loc, vel, c] : zip::Range(locations(), velocities(), counts())) {
    for (int i = 0; i != c; i++) {
      bounds_check(loc[i], vel[i]);
    }
  }
  for (auto &&[loc, vel, c] : zip::Range(locations(), velocities(), counts())) {
    for (int i = 0; i != c; i++) {
      sortSector(loc[i], vel[i], locations(true), velocities(true),
                 counts(true));
    }
  }
}

void WorldState::write(void *dst) {
  size_t i = 0;
  auto range = zip::Range(locations(), velocities(), counts());
  for (auto &&[loc, vel, c] : range) {
    auto span = std::span(loc.data(), c);
    std::memcpy((char *)dst + i, span.data(), span.size_bytes());
    i += span.size_bytes();
  }

  for (auto &c : counts()) {
    c = 0;
  }

  is_swap = !is_swap;
}
void WorldState::print() {
  for (auto &&[loc, vel, c] : zip::Range(locations(), velocities(), counts())) {
    fmt::print("{}, ", std::span(loc.data(), c));
  }
  std::puts("");
}
} // namespace partsim