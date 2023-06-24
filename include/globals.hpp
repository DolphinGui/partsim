#pragma once
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>

inline vk::raii::Context context{};
inline vk::raii::Instance instance{nullptr};