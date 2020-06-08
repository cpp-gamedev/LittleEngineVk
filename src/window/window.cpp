#include <memory>
#include "core/log.hpp"
#include "core/utils.hpp"
#include "engine/window/window.hpp"
#include "window_impl.hpp"

namespace le
{
namespace
{
WindowID g_nextWindowID = WindowID::s_null;
} // namespace

Window::Service::Service()
{
	if (!WindowImpl::init())
	{
		throw std::runtime_error("Failed to initialise Window Service!");
	}
}

Window::Service::~Service()
{
	WindowImpl::deinit();
}

std::array<std::string_view, (size_t)Window::Mode::eCOUNT_> const Window::s_modeNames = {"Decorated Window", "Borderless Window",
																						 "Borderless Fullscreen", "Dedicated Fullscreen"};

std::string const Window::s_tName = utils::tName<Window>();

Window::Window()
{
	m_id = ++g_nextWindowID.handle;
	m_uImpl = std::make_unique<WindowImpl>(this);
	LOG_I("[{}:{}] constructed", s_tName, m_id);
}

Window::Window(Window&&) = default;
Window& Window::operator=(Window&&) = default;
Window::~Window()
{
	m_uImpl.reset();
	LOG_I("[{}:{}] destroyed", s_tName, m_id);
}

void Window::pollEvents()
{
	WindowImpl::pollEvents();
	return;
}

void Window::renderAll()
{
	WindowImpl::renderAll();
	return;
}

OnText::Token Window::registerText(OnText::Callback callback, WindowID window)
{
	return WindowImpl::s_input[window].onText.subscribe(callback);
}

OnInput::Token Window::registerInput(OnInput::Callback callback, WindowID window)
{
	return WindowImpl::s_input[window].onInput.subscribe(callback);
}

OnMouse::Token Window::registerMouse(OnMouse::Callback callback, WindowID window)
{
	return WindowImpl::s_input[window].onMouse.subscribe(callback);
}

OnMouse::Token Window::registerScroll(OnMouse::Callback callback, WindowID window)
{
	return WindowImpl::s_input[window].onScroll.subscribe(callback);
}

WindowID Window::guiWindow()
{
	return WindowImpl::guiWindow();
}

gfx::Renderer const& Window::renderer() const
{
	return m_renderer;
}

gfx::Renderer& Window::renderer()
{
	return m_renderer;
}

WindowID Window::id() const
{
	return m_id;
}

bool Window::isOpen() const
{
	return m_uImpl ? m_uImpl->isOpen() : false;
}

bool Window::isClosing() const
{
	return m_uImpl ? m_uImpl->isClosing() : false;
}

bool Window::isFocused() const
{
	return m_uImpl ? m_uImpl->isFocused() : false;
}

glm::ivec2 Window::windowSize() const
{
	return m_uImpl ? m_uImpl->windowSize() : glm::ivec2(0);
}

glm::ivec2 Window::framebufferSize() const
{
	return m_uImpl ? m_uImpl->framebufferSize() : glm::ivec2(0);
}

bool Window::create(Info const& info)
{
	return m_uImpl ? m_uImpl->create(info) : false;
}

void Window::close()
{
	if (m_uImpl)
	{
		m_uImpl->close();
	}
	return;
}

void Window::destroy()
{
	if (m_uImpl)
	{
		m_uImpl->destroy();
	}
	return;
}

OnText::Token Window::registerText(OnText::Callback callback)
{
	return WindowImpl::s_input[m_id].onText.subscribe(callback);
}

OnInput::Token Window::registerInput(OnInput::Callback callback)
{
	return WindowImpl::s_input[m_id].onInput.subscribe(callback);
}

OnMouse::Token Window::registerMouse(OnMouse::Callback callback)
{
	return WindowImpl::s_input[m_id].onMouse.subscribe(callback);
}

OnMouse::Token Window::registerScroll(OnMouse::Callback callback)
{
	return WindowImpl::s_input[m_id].onScroll.subscribe(callback);
}

OnFiledrop::Token Window::registerFiledrop(OnFiledrop::Callback callback)
{
	return WindowImpl::s_input[m_id].onFiledrop.subscribe(callback);
}

OnFocus::Token Window::registerFocus(OnFocus::Callback callback)
{
	return WindowImpl::s_input[m_id].onFocus.subscribe(callback);
}

OnWindowResize::Token Window::registerResize(OnWindowResize::Callback callback)
{
	return WindowImpl::s_input[m_id].onWindowResize.subscribe(callback);
}

OnClosed::Token Window::registerClosed(OnClosed::Callback callback)
{
	return WindowImpl::s_input[m_id].onClosed.subscribe(callback);
}

void Window::setCursorMode(CursorMode mode) const
{
	if (m_uImpl)
	{
		m_uImpl->setCursorMode(mode);
	}
	return;
}

CursorMode Window::cursorMode() const
{
	return m_uImpl ? m_uImpl->cursorMode() : CursorMode::eDefault;
}

glm::vec2 Window::cursorPos() const
{
	return m_uImpl ? m_uImpl->cursorPos() : glm::vec2(0.0f);
}

void Window::setCursorPos(glm::vec2 const& pos) const
{
	if (m_uImpl)
	{
		m_uImpl->setCursorPos(pos);
	}
	return;
}

std::string Window::clipboard() const
{
	return m_uImpl ? m_uImpl->clipboard() : std::string();
}

JoyState Window::joyState(s32 id)
{
	return WindowImpl::joyState(id);
}

GamepadState Window::gamepadState(s32 id)
{
	return WindowImpl::gamepadState(id);
}

std::vector<GamepadState> Window::activeGamepadStates()
{
	return WindowImpl::activeGamepadStates();
}

f32 Window::triggerToAxis(f32 triggerValue)
{
	return WindowImpl::triggerToAxis(triggerValue);
}

size_t Window::joystickAxesCount(s32 id)
{
	return WindowImpl::joystickAxesCount(id);
}

size_t Window::joysticKButtonsCount(s32 id)
{
	return WindowImpl::joysticKButtonsCount(id);
}

std::string_view Window::toString(s32 key)
{
	return WindowImpl::toString(key);
}
} // namespace le