#pragma once
#if defined(LEVK_EDITOR)
#include <glm/glm.hpp>
#include <core/time.hpp>
#include <engine/gfx/screen_rect.hpp>
#include <engine/window/common.hpp>
#include <engine/game/freecam.hpp>

namespace le::editor
{
inline bool g_bTickGame = true;
inline gfx::ScreenRect g_gameRect = {};
inline FreeCam g_editorCam;

bool init(WindowID editorWindow);
void deinit();
void tick(Time dt);
} // namespace le::editor
#endif
