#include "globals.hpp"
#include <queues.hpp>
#include <vulkan/vulkan_raii.hpp>

vk::raii::Context context{};
vk::raii::Instance instance{nullptr};
vk::raii::DebugUtilsMessengerEXT debug_messager{nullptr};
vk::raii::Device device{nullptr};

Queues queues{};