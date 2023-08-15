#pragma once

#include <span>
#include <stdexcept>
#include <vulkan/vulkan.hpp>

#include "context.hpp"

inline uint32_t findMemoryType(vk::PhysicalDevice phys, uint32_t typeFilter,
                               vk::MemoryPropertyFlags properties) {
  auto memProperties = phys.getMemoryProperties();
  for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
    if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags &
                                    properties) == properties) {
      return i;
    }
  }

  throw std::runtime_error("failed to find suitable memory type!");
}

struct Buffer {
  vk::Buffer buffer{};
  vk::DeviceMemory mem{};
  vk::Device d{};
  Buffer(vk::Device device, vk::PhysicalDevice phys, size_t size,
         vk::BufferUsageFlags usage, vk::MemoryPropertyFlags properties) {
    d = device;
    buffer = d.createBuffer({.size = size,
                             .usage = usage,
                             .sharingMode = vk::SharingMode::eExclusive});
    auto mem_reqs = d.getBufferMemoryRequirements(buffer);

    mem = d.allocateMemory({.allocationSize = mem_reqs.size,
                            .memoryTypeIndex = findMemoryType(
                                phys, mem_reqs.memoryTypeBits, properties)});
    d.bindBufferMemory(buffer, mem, 0);
  }

  Buffer(Buffer &&other) {
    buffer = other.buffer;
    mem = other.mem;
    d = other.d;
    other.buffer = nullptr;
    other.mem = nullptr;
    other.d = nullptr;
  }
  ~Buffer() {
    if (d) {
      d.destroyBuffer(buffer);
      d.freeMemory(mem);
    }
  }

  void write(vk::Device device, vk::PhysicalDevice phys, Renderer &vk,
             std::span<const uint8_t> data) {

    using enum vk::BufferUsageFlagBits;
    using enum vk::MemoryPropertyFlagBits;
    auto staging = Buffer(device, phys, data.size(), eTransferSrc,
                          eHostVisible | eHostCoherent);

    auto ptr = staging.d.mapMemory(staging.mem, 0, data.size());
    std::memcpy(ptr, data.data(), data.size());
    staging.d.unmapMemory(staging.mem);

    auto cmdBuffers = vk.getCommands(1);
    vk::CommandBufferBeginInfo info{};
    vkassert(cmdBuffers[0].begin(&info));
    cmdBuffers[0].copyBuffer(staging.buffer, buffer,
                             vk::BufferCopy{0, 0, data.size()});
    cmdBuffers[0].end();
    vk.queues.mem().submit(std::array{vk::SubmitInfo{
        .commandBufferCount = 1, .pCommandBuffers = cmdBuffers.data()}});
    vk.queues.mem().waitIdle();
  }
};

template <typename T> struct MappedBuffer {
  Buffer buffer;
  void *mapped = nullptr;
  MappedBuffer() = delete;
  MappedBuffer(
      vk::Device d, vk::PhysicalDevice phys,
      vk::BufferUsageFlags type = vk::BufferUsageFlagBits::eUniformBuffer)
      : buffer(d, phys, sizeof(T), type,
               vk::MemoryPropertyFlagBits::eHostCoherent |
                   vk::MemoryPropertyFlagBits::eHostVisible) {
    mapped = buffer.d.mapMemory(buffer.mem, 0, sizeof(T));
  }

  MappedBuffer(MappedBuffer &&other)
      : buffer(std::move(other.buffer)), mapped(other.mapped) {}

  ~MappedBuffer() {
    if (buffer.d)
      buffer.d.unmapMemory(buffer.mem);
  }

  void write(const T &data) { std::memcpy(mapped, &data, sizeof(T)); }
  void read(T *out) { std::memcpy(out, mapped, sizeof(T)); }
};
