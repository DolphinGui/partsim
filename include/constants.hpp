#pragma once

#include <chrono>

namespace world {
using namespace std::chrono_literals;

using FTime = std::chrono::duration<float, std::chrono::seconds::period>;
constexpr auto delta = FTime(1s) / 120;
constexpr size_t object_count = 200;
constexpr float max_x = 100, max_y = 100;
constexpr float radius = 1.0;
struct constants_t {
  const int obj_count = object_count;
  const float max_x = world::max_x, max_y = world::max_y;
} inline constants;

} // namespace world