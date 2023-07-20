#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <fmt/core.h>
#include <fmt/ranges.h>
#include <numeric>
#include <ranges>
#include <stdexcept>
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
  return {1.0 * signrand() / float(RAND_MAX),
          1.0 * signrand() / float(RAND_MAX)};
}
int indexVec(glm::vec2 s, WorldState &w) {
  int i = int(s.x / (WorldState::max_x / WorldState::sector_count_x)) +
          int(s.y / (WorldState::max_y / WorldState::sector_count_y)) *
              WorldState::sector_count_x;
  if (i > WorldState::sector_count || i < 0) {
    fmt::print("i: {}, s: {}\n", i, s);
    throw std::runtime_error("out of bounds index");
  }
  return i;
}
} // namespace
WorldState::WorldState() {
  auto n = 1689711473;
  fmt::print("seed: {}\n", n);
  std::srand(n);
  locations_vec.resize(sector_count * 2, {});
  velocities_vec.resize(sector_count * 2, {});
  counts_vec.resize(sector_count * 2, 0);
  extra_locations.reserve(20);
  extra_velocities.reserve(20);
  for (int _ = 0; _ != objects; _++) {
    auto gen = gen_s(max_x / 2, max_y / 2);
    int index = indexVec(gen, *this);
    size_t &i = counts()[index];
    if (i >= sector_size) {
      extra_locations.push_back(gen);
      extra_velocities.push_back(gen_v());
    } else {
      location_sectors()[index][i] = gen;
      velocity_sectors()[index][i] = gen_v();
      i++;
    }
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

using VecSpan = std::span<glm::vec2>;

inline void collision_check_sector(VecSpan pos, VecSpan vel, WorldState &w) {
  auto size = pos.size();
  if (size == 0)
    return;
  for (int a = 0; a != size - 1; a++) {
    for (int b = a + 1; b != size; b++) {
      collision_check(pos[a], vel[a], pos[b], vel[b]);
    }
  }

  [[unlikely]] if (!w.extra_locations.empty())
    for (auto &&[e_s, e_v] :
         zip::Range(w.extra_locations, w.extra_velocities)) {
      for (auto &&[s, v] : zip::Range(pos, vel)) {
        collision_check(e_s, e_v, s, v);
      }
    }
}
inline void collision_check_sectors(VecSpan pos_a, VecSpan vel_a, VecSpan pos_b,
                                    VecSpan vel_b, WorldState &w) {
  for (auto &&[a_s, a_v] : zip::Range(pos_a, vel_a)) {
    for (auto &&[b_s, b_v] : zip::Range(pos_b, vel_b)) {
      collision_check(a_s, a_v, b_s, b_v);
    }
  }
}

inline void sortSector(glm::vec2 s, glm::vec2 v,
                       std::span<WorldState::Sector> other_loc,
                       std::span<WorldState::Sector> other_vec,
                       std::span<size_t> other_count, WorldState &w) {
  int index = indexVec(s, w);
  auto &count = other_count[index];
  [[unlikely]] if (count > WorldState::sector_size - 1) {
    w.extra_locations.push_back(s);
    w.extra_velocities.push_back(v);
    return;
  }
  assert(index < other_loc.size());
  assert(count < WorldState::sector_size);
  other_loc[index][count] = s;
  other_vec[index][count] = v;
  count++;
}

inline void sortExtras(glm::vec2 s, glm::vec2 v,
                       std::span<WorldState::Sector> other_loc,
                       std::span<WorldState::Sector> other_vec,
                       std::span<size_t> other_count, WorldState &w) {
  int index = indexVec(s, w);
  auto &count = other_count[index];
  [[unlikely]] if (count > WorldState::sector_size) {
    w.extra_locations.push_back(s);
    w.extra_velocities.push_back(v);
    return;
  }
  other_loc[index][count] = s;
  other_vec[index][count] = v;
  count++;
}

} // namespace

void WorldState::process() {
  for (int i = 0; i != sector_count; i++) {
    for (auto &&[s, v] : zip::Range(locations(i), velocities(i))) {
      s += v;
    }
  }

  [[unlikely]] if (!extra_locations.empty()) {
    for (auto &&[s, v] : zip::Range(extra_locations, extra_velocities)) {
      s += v;
    }
  }
  // later optimize so it only bounds checks the outer sectors
  for (int i = 0; i != sector_count; i++) {
    for (auto &&[s, v] : zip::Range(locations(i), velocities(i))) {
      bounds_check(s, v);
    }
  }

  [[unlikely]] if (!extra_locations.empty()) {
    for (auto &&[s, v] : zip::Range(extra_locations, extra_velocities)) {
      bounds_check(s, v);
    }
  }

  for (int sector = 0; sector != sector_count - sector_count_x - 1; sector++) {
    collision_check_sector(locations(sector), velocities(sector), *this);
    collision_check_sectors(locations(sector), velocities(sector),
                            locations(sector + 1), velocities(sector + 1),
                            *this);
    collision_check_sectors(locations(sector), velocities(sector),
                            locations(sector + sector_count_x),
                            velocities(sector + sector_count_x), *this);
  }

  for (int sector = sector_count_x - 1; sector != sector_count - 1;
       sector += sector_count_x) {
    collision_check_sector(locations(sector), velocities(sector), *this);
    collision_check_sectors(locations(sector), velocities(sector),
                            locations(sector + sector_count_x),
                            velocities(sector + sector_count_x), *this);
  }

  for (int sector = sector_count - sector_count_x; sector != sector_count - 1;
       sector++) {
    collision_check_sector(locations(sector), velocities(sector), *this);
    collision_check_sectors(locations(sector), velocities(sector),
                            locations(sector + 1), velocities(sector + 1),
                            *this);
  }

  collision_check_sector(locations(sector_count - 1),
                         velocities(sector_count - 1), *this);

  auto size = extra_locations.size();
  for (int i = 0; i != size; i++) {
    int index = indexVec(extra_locations[i], *this);
    auto &count = counts()[index];
    [[likely]] if (count >= sector_size - 1) { continue; }
    location_sectors()[index][count] = extra_locations[i];
    velocity_sectors()[index][count] = extra_velocities[i];
    count++;
    extra_locations[i] = extra_locations.back();
    extra_velocities[i] = extra_velocities.back();
    extra_locations.pop_back();
    extra_velocities.pop_back();
  }

  for (int sector = 0; sector != sector_count; sector++) {
    auto range = zip::Range(locations(sector), velocities(sector));
    for (auto &&[s, v] : range) {
      sortSector(s, v, location_sectors(true), velocity_sectors(true),
                 counts(true), *this);
    }
  }
}

void WorldState::write(void *dst) {
  size_t i = 0;
  for (auto &&[loc, vel, c] :
       zip::Range(location_sectors(), velocity_sectors(), counts())) {
    auto span = std::span(loc.data(), c);
    std::memcpy((char *)dst + i, span.data(), span.size_bytes());
    i += span.size_bytes();
  }

  [[unlikely]] if (!extra_locations.empty())
    std::memcpy((char *)dst + i, extra_locations.data(),
                extra_locations.size() * sizeof(glm::vec2));

  for (auto &c : counts()) {
    c = 0;
  }

  is_swap = !is_swap;
}
void WorldState::print() {
  for (int i = 0; i != sector_count; i++) {
    fmt::print("sector {}: {}\n", i, locations(i));
  }
}
} // namespace partsim