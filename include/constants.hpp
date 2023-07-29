#pragma once

#include <chrono>

namespace world {
using namespace std::chrono_literals;

constexpr static size_t object_count = 4;
using FTime = std::chrono::duration<float, std::chrono::seconds::period>;
constexpr auto delta = FTime(1s) / 60;
constexpr float max_x = 50, max_y = 30;

} // namespace world