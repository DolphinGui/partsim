#include "vkformat.hpp"

fmt::basic_format_context<fmt::appender, char>::iterator
fmt::formatter<vk::DebugUtilsMessageSeverityFlagBitsEXT>::format(
    vk::DebugUtilsMessageSeverityFlagBitsEXT c,
    fmt::format_context &ctx) const {
  string_view name = "unknown";
  using enum vk::DebugUtilsMessageSeverityFlagBitsEXT;
  switch (c) {
  case eVerbose:
    name = "V";
    break;
  case eInfo:
    name = "I";
    break;
  case eWarning:
    name = "W";
    break;
  case eError:
    name = "E";
    break;
  }
  return formatter<string_view>::format(name, ctx);
}

fmt::basic_format_context<fmt::appender, char>::iterator
fmt::formatter<vk::DebugUtilsMessageTypeFlagBitsEXT>::format(
    vk::DebugUtilsMessageTypeFlagBitsEXT c, fmt::format_context &ctx) const {
  string_view name = "unknown";
  using enum vk::DebugUtilsMessageTypeFlagBitsEXT;
  switch (c) {
  case eGeneral:
    name = "General";
    break;
  case eValidation:
    name = "Validation";
    break;
  case ePerformance:
    name = "Performance";
    break;
  case eDeviceAddressBinding:
    name = "Address Binding";
    break;
  }
  return formatter<string_view>::format(name, ctx);
}