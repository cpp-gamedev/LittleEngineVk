#pragma once
#include <core/span.hpp>
#include <glm/vec2.hpp>

namespace le::graphics {
template <typename T>
struct TBitmap {
	using type = T;

	T bytes{};
	glm::ivec2 size{};
};

using Bitmap = TBitmap<bytearray>;
using BMPview = View<Bitmap::type::value_type>;
} // namespace le::graphics
