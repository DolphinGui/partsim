#pragma once

#include <chrono>
#include <cmath>
#include <glm/vec2.hpp>
#include <ratio>
#include <vector>

#include "constmath.hpp"
#include "vertex.hpp"

namespace partsim {
namespace chron = std::chrono;
using ftime = chron::duration<float>;

constexpr float tick_rate = 30.0;
constexpr auto world_delta = ftime(chron::seconds(1)) / tick_rate;

constexpr inline int calcSectorSize(int objects, int sectors) {
  return objects / sectors +
         constmath::ceil(3.5 * constmath::sqrt(double(objects) / sectors *
                                               (1.0 - 1.0 / sectors)));
}

struct WorldState {
  constexpr static int objects = 30;
  constexpr static int sector_count_x = 2, sector_count_y = 2,
                       sector_count = sector_count_x * sector_count_y;
  using Sector = std::array<glm::vec2, calcSectorSize(objects, sector_count)>;

  constexpr static float max_x = 100, max_y = 60;
  constexpr static float radius = 1.0;
  std::vector<Sector> locations;
  std::vector<Sector> velocities;
  std::vector<size_t> counts;
  std::vector<glm::vec2> extra_locations;
  std::vector<glm::vec2> extra_velocities;

  explicit WorldState();
  void process();
  void write(void *dst) const;

  inline glm::vec2 &getS(int index) {
    return locations.at(index)[counts[index]];
  }
  inline glm::vec2 &getV(int index) {
    return velocities.at(index)[counts[index]];
  }
};

} // namespace partsim