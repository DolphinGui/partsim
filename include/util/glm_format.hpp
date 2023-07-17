#pragma once

#include <fmt/core.h>
#include <glm/vec2.hpp>

// literally copied from fmt website
template <> struct fmt::formatter<glm::vec2> {
  // Presentation format: 'f' - fixed, 'e' - exponential.
  char presentation = 'f';

  constexpr auto parse(format_parse_context &ctx)
      -> format_parse_context::iterator {
    auto it = ctx.begin(), end = ctx.end();
    if (it != end && (*it == 'f' || *it == 'e'))
      presentation = *it++;

    if (it != end && *it != '}')
      ctx.on_error("invalid format");

    return it;
  }

  // Formats the point p using the parsed format specification (presentation)
  // stored in this formatter.
  auto format(const glm::vec2 &p, format_context &ctx) const
      -> format_context::iterator {
    // ctx.out() is an output iterator to write to.
    return presentation == 'f'
               ? fmt::format_to(ctx.out(), "({:.1f}, {:.1f})", p.x, p.y)
               : fmt::format_to(ctx.out(), "({:.1e}, {:.1e})", p.x, p.y);
  }
};
