#pragma once
#include <utility>
#include "core/utils.hpp"
#include "common.hpp"

namespace le::gfx
{
constexpr bool g_VRAM_bLogAllocs = true;

struct ImageInfo final
{
	vk::ImageCreateInfo createInfo;
	std::string name;
	QFlags queueFlags = QFlags({QFlag::eGraphics, QFlag::eTransfer});
	VmaMemoryUsage vmaUsage = VMA_MEMORY_USAGE_GPU_ONLY;
};

struct BufferInfo final
{
	std::string name;
	vk::DeviceSize size;
	vk::BufferUsageFlags usage;
	vk::MemoryPropertyFlags properties;
	QFlags queueFlags = QFlags({QFlag::eGraphics, QFlag::eTransfer});
	VmaMemoryUsage vmaUsage = VMA_MEMORY_USAGE_GPU_ONLY;
};

namespace vram
{
inline VmaAllocator g_allocator;

void init();
void deinit();

Buffer createBuffer(BufferInfo const& info, bool bSilent = false);
bool write(Buffer buffer, void const* pData, vk::DeviceSize size = 0);
[[nodiscard]] vk::Fence copy(Buffer const& src, Buffer const& dst, vk::DeviceSize size = 0);
[[nodiscard]] vk::Fence stage(Buffer const& deviceBuffer, void const* pData, vk::DeviceSize size = 0);

[[nodiscard]] vk::Fence copy(ArrayView<u8> pixels, Image const& dst, std::pair<vk::ImageLayout, vk::ImageLayout> layouts);

Image createImage(ImageInfo const& info);

void release(Buffer buffer, bool bSilent = false);
void release(Image image);

template <typename T1, typename... Tn>
void release(T1 t1, Tn... tn)
{
	static_assert(std::is_same_v<T1, Image> || std::is_same_v<T1, Buffer>, "Invalid Type!");
	release(t1);
	release(tn...);
}
} // namespace vram
} // namespace le::gfx