#pragma once

#include <vulkan/vulkan.hpp>

inline void vkassert(vk::Result r) {
  if (r != vk::Result::eSuccess && r != vk::Result::eSuboptimalKHR)
    throw std::runtime_error(vk::to_string(r));
}

inline void vkassert(VkResult result) {
  vk::Result r = vk::Result(result);
  if (r != vk::Result::eSuccess && r != vk::Result::eSuboptimalKHR)
    throw std::runtime_error(vk::to_string(r));
}