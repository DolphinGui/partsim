#include <cstdint>
#include <span>

#include "context.hpp"

namespace {
#include "build/comp.hpp"
#include "build/frag.hpp"
#include "build/vert.hpp"
} // namespace

namespace shaders {
std::span<const uint32_t> vertex() { return vert; }
std::span<const uint32_t> fragment() { return frag; }
std::span<const uint32_t> compute() { return comp; }
} // namespace shaders