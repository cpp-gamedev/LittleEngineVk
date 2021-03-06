#pragma once
#include <glm/gtx/quaternion.hpp>
#include <glm/vec3.hpp>

namespace le::graphics {
constexpr glm::vec3 up = {0.0f, 1.0f, 0.0f};
constexpr glm::vec3 right = {1.0f, 0.0f, 0.0f};
constexpr glm::vec3 front = {0.0f, 0.0f, 1.0f};
constexpr glm::quat identity = {1.0f, 0.0f, 0.0f, 0.0f};
} // namespace le::graphics
