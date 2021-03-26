#include <core/ensure.hpp>
#include <core/log.hpp>
#include <core/maths.hpp>
#include <engine/game/freecam.hpp>
#include <engine/levk.hpp>
#include <game/input_impl.hpp>
#include <glm/gtx/quaternion.hpp>

namespace le {
using namespace input;

#if defined(LEVK_EDITOR)
void FreeCam::init(bool bEditorContext)
#else
void FreeCam::init()
#endif
{
	m_input = {};
	m_input.mapTrigger("look_toggle", [this]() {
		if (m_state.flags.test(Flag::eEnabled) && m_state.flags.test(Flag::eKeyToggle_Look)) {
			m_state.flags.flip(Flag::eKeyLook);
			m_state.flags.flip(Flag::eLooking);
			m_state.flags.reset(Flag::eTracking);
		}
	});
	m_input.addTrigger("look_toggle", m_config.lookToggle.key, m_config.lookToggle.action, m_config.lookToggle.mods);

	m_input.mapState("looking", [this](bool bActive) {
		if (m_state.flags.test(Flag::eEnabled) && !m_state.flags.test(Flag::eKeyLook)) {
			if (!bActive || !m_state.flags.test(Flag::eLooking)) {
				m_state.flags.reset(Flag::eTracking);
			}
			m_state.flags[Flag::eLooking] = bActive;
		}
	});
	m_input.addState("looking", Key::eMouseButton2);

	m_input.mapRange("look_x", [this](f32 value) {
		if (m_state.flags.test(Flag::eEnabled)) {
			m_padLook.x = value;
		}
	});
	m_input.mapRange("look_y", [this](f32 value) {
		if (m_state.flags.test(Flag::eEnabled)) {
			m_padLook.y = value;
		}
	});
	m_input.addRange("look_x", Axis::eRightX);
	m_input.addRange("look_y", Axis::eRightY);

	m_input.mapTrigger("reset_speed", [this]() {
		if (m_state.flags.test(Flag::eEnabled)) {
			m_state.speed = m_config.defaultSpeed;
		}
	});
	m_input.addTrigger("reset_speed", Key::eMouseButton3);

	m_input.mapRange("speed", [this](f32 value) {
		if (m_state.flags.test(Flag::eEnabled) && !m_state.flags.test(Flag::eFixedSpeed)) {
			m_state.dSpeed += (value * 0.1f);
		}
	});
	m_input.addRange("speed", Axis::eMouseScrollY);
	m_input.addRange("speed", Key::eGamepadButtonLeftBumper, Key::eGamepadButtonRightBumper);

	m_input.mapRange("move_x", [this](f32 value) {
		if (m_state.flags.test(Flag::eEnabled) && value * value > m_config.padStickEpsilon) {
			m_dXZ.x = value;
		}
	});
	m_input.mapRange("move_y", [this](f32 value) {
		if (m_state.flags.test(Flag::eEnabled) && value * value > m_config.padStickEpsilon) {
			m_dXZ.y = -value;
		}
	});
	m_input.addRange("move_x", Axis::eLeftX);
	m_input.addRange("move_x", Key::eLeft, Key::eRight);
	m_input.addRange("move_x", Key::eA, Key::eD);
	m_input.addRange("move_y", Axis::eLeftY, true);
	m_input.addRange("move_y", Key::eDown, Key::eUp);
	m_input.addRange("move_y", Key::eS, Key::eW);

	m_input.mapRange("elevation_up", [this](f32 value) {
		if (m_state.flags.test(Flag::eEnabled)) {
			m_dY.x = value;
		}
	});
	m_input.mapRange("elevation_down", [this](f32 value) {
		if (m_state.flags.test(Flag::eEnabled)) {
			m_dY.y = value;
		}
	});
	m_input.addRange("elevation_up", Axis::eLeftTrigger);
	m_input.addRange("elevation_down", Axis::eRightTrigger);

#if defined(LEVK_EDITOR)
	if (bEditorContext) {
		m_token = input::registerEditorContext(&m_input);
	} else
#endif
	{
		m_token = input::registerContext(&m_input);
	}

	m_state.speed = m_config.defaultSpeed;
	m_state.flags.set(Flag::eEnabled);
	return;
}

void FreeCam::tick(Time_s dt) {
	if (!m_state.flags.test(Flag::eEnabled)) {
		return;
	}

	if (auto pWindow = engine::mainWindow()) {
		pWindow->setCursorMode(m_state.flags.test(Flag::eLooking) ? CursorMode::eDisabled : CursorMode::eDefault);
	}

	if (!input::focused() || !m_input.wasFired()) {
		m_state.flags.reset(Flag::eTracking);
		return;
	}

	f32 const dt_s = dt.count();
	// Speed
	if (!m_state.flags.test(Flag::eFixedSpeed)) {
		if (m_state.dSpeed * m_state.dSpeed > 0.0f) {
			m_state.speed = std::clamp(m_state.speed + (m_state.dSpeed * dt_s * 100), m_config.minSpeed, m_config.maxSpeed);
			m_state.dSpeed = maths::lerp(m_state.dSpeed, 0.0f, 0.75f);
			if (m_state.dSpeed * m_state.dSpeed < 0.01f) {
				m_state.dSpeed = 0.0f;
			}
		}
	}

	// Elevation
	f32 const dY = m_dY.x - m_dY.y;
	if (std::abs(dY) > 0.01f) {
		m_camera.position.y += (dY * dt_s * m_state.speed);
	}

	// Look
	if (m_state.flags.test(Flag::eEnabled) && m_state.flags.test(Flag::eLooking)) {
		m_state.cursorPos.second = input::screenToWorld(input::cursorPosition(true));
		if (!m_state.flags.test(Flag::eTracking)) {
			m_state.cursorPos.first = m_state.cursorPos.second;
			m_state.flags.set(Flag::eTracking);
		}
	}
	f32 dLook = m_config.padLookSens * dt_s;
	if (glm::length2(m_padLook) > m_config.padStickEpsilon) {
		m_state.yaw += (m_padLook.x * dLook);
		m_state.pitch += (-m_padLook.y * dLook);
		m_padLook = {};
	}

	dLook = m_config.mouseLookSens;
	glm::vec2 const dCursorPos = m_state.cursorPos.second - m_state.cursorPos.first;
	if (glm::length2(dCursorPos) > m_config.mouseLookEpsilon) {
		m_state.yaw += (dCursorPos.x * dLook);
		m_state.pitch += (dCursorPos.y * dLook);
		m_state.cursorPos.first = m_state.cursorPos.second;
	}
	glm::quat const pitch = glm::angleAxis(glm::radians(m_state.pitch), gfx::g_nRight);
	glm::quat const yaw = glm::angleAxis(glm::radians(m_state.yaw), -gfx::g_nUp);
	m_camera.orientation = yaw * pitch;

	// Move
	glm::vec3 dPos = {};
	glm::vec3 const nForward = glm::normalize(glm::rotate(m_camera.orientation, -gfx::g_nFront));
	glm::vec3 const nRight = glm::normalize(glm::rotate(m_camera.orientation, gfx::g_nRight));

	if (glm::length2(m_dXZ) > 0.0f) {
		m_dXZ = glm::normalize(m_dXZ);
		dPos += (nRight * m_dXZ.x);
		dPos += (nForward * -m_dXZ.y);
		m_camera.position += (dPos * dt_s * m_state.speed);
		m_dXZ = {};
	}
	return;
}

void FreeCam::reset() {
	m_camera.reset();
	m_state.dSpeed = 0.0f;
	m_state.pitch = m_state.yaw = 0.0f;
	m_state.heldKeys.clear();
	m_state.flags.reset(Flags(Flag::eTracking) | Flag::eLooking);
	return;
}
} // namespace le
