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
  return {1.0 * signrand() / float(RAND_MAX),
          1.0 * signrand() / float(RAND_MAX)};
}
int indexVec(glm::vec2 s) {
  return int(s.x / (WorldState::max_x / WorldState::sector_count_x)) +
         int(s.y / (WorldState::max_y / WorldState::sector_count_y)) *
             WorldState::sector_count_x;
}
} // namespace
WorldState::WorldState() {
  auto n = std::time(nullptr);
  fmt::print("seed: {}\n", n);
  std::srand(n);
  locations_vec.resize(sector_count * 2, {});
  velocities_vec.resize(sector_count * 2, {});
  counts_vec.resize(sector_count * 2, 0);
  for (int _ = 0; _ != objects; _++) {
    auto gen = gen_s(max_x, max_y);
    int index = indexVec(gen);
    size_t &i = counts()[index];
    location_sectors()[index][i] = gen;
    velocity_sectors()[index][i] = gen_v();
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

using VecSpan = std::span<glm::vec2>;

inline void collision_check_sector(VecSpan pos, VecSpan vel) {
  auto size = pos.size();
  if (size == 0)
    return;
  for (int a = 0; a != size - 1; a++) {
    for (int b = a + 1; b != size; b++) {
      collision_check(pos[a], vel[a], pos[b], vel[b]);
    }
  }
}
inline void collision_check_sectors(VecSpan pos_a, VecSpan vel_a, VecSpan pos_b,
                                    VecSpan vel_b) {
  for (auto &&[a_s, a_v] : zip::Range(pos_a, vel_a)) {
    for (auto &&[b_s, b_v] : zip::Range(pos_b, vel_b)) {
      collision_check(a_s, a_v, b_s, b_v);
    }
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
  for (int i = 0; i != sector_count; i++) {
    for (auto &&[s, v] : zip::Range(locations(i), velocities(i))) {
      s += v;
    }
  }

  // later optimize so it only bounds checks the outer sectors
  for (int i = 0; i != sector_count; i++) {
    for (auto &&[s, v] : zip::Range(locations(i), velocities(i))) {
      bounds_check(s, v);
    }
  }

  for (int sector = 0; sector != sector_count - sector_count_x - 1; sector++) {
    collision_check_sector(locations(sector), velocities(sector));
    collision_check_sectors(locations(sector), velocities(sector),
                            locations(sector + 1), velocities(sector + 1));
    collision_check_sectors(locations(sector), velocities(sector),
                            locations(sector + sector_count_x),
                            velocities(sector + sector_count_x));
  }

  for (int sector = sector_count_x - 1; sector != sector_count - 1;
       sector += sector_count_x) {
    collision_check_sector(locations(sector), velocities(sector));
    collision_check_sectors(locations(sector), velocities(sector),
                            locations(sector + sector_count_x),
                            velocities(sector + sector_count_x));
  }

  for (int sector = sector_count - sector_count_x; sector != sector_count;
       sector++) {
    collision_check_sector(locations(sector), velocities(sector));
    collision_check_sectors(locations(sector), velocities(sector),
                            locations(sector + 1), velocities(sector + 1));
  }

  collision_check_sector(locations(sector_count - 1),
                         velocities(sector_count - 1));

  for (auto &&[loc, vel, c] :
       zip::Range(location_sectors(), velocity_sectors(), counts())) {
    for (int i = 0; i != c; i++) {
      sortSector(loc[i], vel[i], location_sectors(true), velocity_sectors(true),
                 counts(true));
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