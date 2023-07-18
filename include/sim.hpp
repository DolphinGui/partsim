#pragma once

#include <chrono>
#include <cmath>
#include <glm/vec2.hpp>
#include <ratio>
#include <span>
#include <vector>

#include "util/constmath.hpp"
#include "vertex.hpp"

namespace partsim {
namespace chron = std::chrono;
using ftime = chron::duration<float>;

constexpr float tick_rate = 60.0;
constexpr auto world_delta = ftime(chron::seconds(1)) / tick_rate;

constexpr inline int calcSectorSize(int objects, int sectors) {
  return objects / sectors +
         constmath::ceil(3.5 * constmath::sqrt(double(objects) / sectors *
                                               (1.0 - 1.0 / sectors)));
}

struct WorldState {
  constexpr static int objects = 30;
  constexpr static int sector_count_x = 3, sector_count_y = 3,
                       sector_count = sector_count_x * sector_count_y;
  using Sector = std::array<glm::vec2, calcSectorSize(objects, sector_count)>;

  constexpr static float max_x = 100, max_y = 60;
  constexpr static float radius = 1.0;
  std::vector<Sector> locations_vec;
  std::vector<Sector> velocities_vec;
  std::vector<size_t> counts_vec;
  std::vector<glm::vec2> extra_locations;
  std::vector<glm::vec2> extra_velocities;

  bool is_swap = false;

  explicit WorldState();
  void process();
  void write(void *dst);

  inline std::span<glm::vec2> locations(size_t index) {
    auto i = index + (is_swap ? sector_count : 0);
    return std::span(locations_vec[i].data(), counts_vec[i]);
  }

  inline std::span<Sector> location_sectors(bool swap = false) {
    auto offset = (is_swap != swap) ? sector_count : 0;
    return std::span(locations_vec.data() + offset, sector_count);
  }

  inline std::span<glm::vec2> velocities(size_t index) {
    auto i = index + (is_swap ? sector_count : 0);
    return std::span(velocities_vec[i].data(), counts_vec[i]);
  }

  inline std::span<Sector> velocity_sectors(bool swap = false) {
    auto offset = (is_swap != swap) ? sector_count : 0;
    return std::span(velocities_vec.data() + offset, sector_count);
  }

  inline std::span<size_t> counts(bool swap = false) {
    auto offset = (is_swap != swap) ? sector_count : 0;
    return std::span(counts_vec.data() + offset, sector_count);
  }
  void print();
};

} // namespace partsim