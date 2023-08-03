#pragma once

#include <chrono>

namespace world {
using namespace std::chrono_literals;

using FTime = std::chrono::duration<float, std::chrono::seconds::period>;
constexpr auto delta = FTime(1s) / 60;
constexpr size_t object_count = 5000;
constexpr float max_x = 500, max_y = 500;
struct constants_t {
  const int obj_count = object_count;
  const float max_x = world::max_x, max_y = world::max_y;
} inline constants;

} // namespace world