#pragma once

#include <cinttypes>
#include <limits>

namespace detail {
double constexpr sqrtNewtonRaphson(double x, double curr, double prev) {
  return curr == prev ? curr
                      : sqrtNewtonRaphson(x, 0.5 * (curr + x / curr), curr);
}
} // namespace detail

namespace constmath {
double constexpr sqrt(double x) {
  return x >= 0 && x < std::numeric_limits<double>::infinity()
             ? detail::sqrtNewtonRaphson(x, x, 0)
             : std::numeric_limits<double>::quiet_NaN();
}
constexpr int32_t ceil(double num) {
  return (static_cast<double>(static_cast<int32_t>(num)) == num)
             ? static_cast<int32_t>(num)
             : static_cast<int32_t>(num) + ((num > 0) ? 1 : 0);
}
} // namespace constmath