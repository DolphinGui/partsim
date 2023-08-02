#pragma once

#include <chrono>

namespace world {
using namespace std::chrono_literals;

constexpr static size_t object_count = 500;
using FTime = std::chrono::duration<float, std::chrono::seconds::period>;
constexpr auto delta = FTime(1s) / 60;
constexpr float max_x = 300, max_y = 300;

} // namespace world