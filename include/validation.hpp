#pragma once

#include <cstdint>
#include <vulkan/vulkan.hpp>

constexpr inline std::array validationLayers = {"VK_LAYER_KHRONOS_validation"};

inline bool check_validation_support() {
  uint32_t layerCount;
  vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

  std::vector<VkLayerProperties> availableLayers(layerCount);
  vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

  for (const char *layerName : validationLayers) {
    bool layerFound = false;

    for (const auto &layerProperties : availableLayers) {
      if (strcmp(layerName, layerProperties.layerName) == 0) {
        layerFound = true;
        break;
      }
    }

    if (!layerFound) {
      return false;
    }
  }

  return true;
}

constexpr inline bool enableValidation =
#ifdef NDEBUG
    false;
#else
    true;
#endif
