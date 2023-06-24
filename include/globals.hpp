#pragma once

namespace vk::raii {
struct Context;
struct Instance;
struct DebugUtilsMessengerEXT;
struct PhysicalDevices;
struct Device;
} // namespace vk::raii
struct Queues;
struct GLFWwin;

extern vk::raii::Context context;
extern vk::raii::Instance instance;
extern vk::raii::DebugUtilsMessengerEXT debug_messager;
extern vk::raii::Device device;
extern Queues queues;
extern GLFWwin window;