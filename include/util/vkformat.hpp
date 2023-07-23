#pragma once

#include <fmt/format.h>
#include <string_view>
#include <vulkan/vulkan.hpp>

template <>
struct fmt::formatter<vk::DebugUtilsMessageSeverityFlagBitsEXT>
    : fmt::formatter<std::string_view> {
  fmt::basic_format_context<fmt::appender, char>::iterator
  format(vk::DebugUtilsMessageSeverityFlagBitsEXT, format_context &) const;
};

template <>
struct fmt::formatter<vk::DebugUtilsMessageTypeFlagBitsEXT>
    : fmt::formatter<std::string_view> {
  fmt::basic_format_context<fmt::appender, char>::iterator
  format(vk::DebugUtilsMessageTypeFlagBitsEXT, format_context &) const;
};