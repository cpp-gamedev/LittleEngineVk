#pragma once
#include <core/io.hpp>
#include <core/version.hpp>
#include <engine/editor/editor.hpp>
#include <engine/input/driver.hpp>
#include <engine/input/receiver.hpp>
#include <engine/tagged_deque.hpp>
#include <graphics/context/bootstrap.hpp>
#include <graphics/render_context.hpp>
#include <levk_imgui/levk_imgui.hpp>

namespace le {
namespace window {
class IInstance;
class DesktopInstance;
} // namespace window

class Engine {
  public:
	using Window = window::IInstance;
	using Desktop = window::DesktopInstance;
	using Boot = graphics::Bootstrap;
	using Context = graphics::RenderContext;

	struct GFX {
		Boot boot;
		Context context;
		DearImGui imgui;

		GFX(Window const& winst, Boot::CreateInfo const& bci);

	  private:
		static Boot::MakeSurface makeSurface(Window const& winst);
	};

	struct Stats {
		struct Frame {
			Time_s ft;
			u32 rate;
			u64 count;
		};
		struct Gfx {
			struct {
				u64 buffers;
				u64 images;
			} bytes;
			u32 drawCalls;
			u32 triCount;
		};

		Frame frame;
		Gfx gfx;
		Time_s upTime;
	};

	struct CreateInfo;
	struct DrawFrame;

	static Version version() noexcept;
	static Stats const& stats() noexcept;

	Engine(Window& winInst, CreateInfo const& info);

	input::Driver::Out poll(bool consume) noexcept;
	void pushReceiver(input::Receiver& context);

	bool editorActive() const noexcept;
	bool editorEngaged() const noexcept;

	bool beginFrame(bool waitDrawReady);
	bool drawReady();
	std::optional<Context::Frame> beginDraw(Colour clear = colours::black, vk::ClearDepthStencilValue depth = {1.0f, 0});
	std::optional<DrawFrame> drawFrame(Colour clear = colours::black, vk::ClearDepthStencilValue depth = {1.0f, 0});
	bool endDraw(Context::Frame const& frame);

	bool boot(Boot::CreateInfo const& boot);
	bool unboot() noexcept;
	bool booted() const noexcept;

	GFX& gfx();
	GFX const& gfx() const;
	input::State const& inputState() const noexcept;
	Desktop* desktop() const noexcept;

	glm::ivec2 framebufferSize() const noexcept;
	vk::Viewport viewport(Viewport const& view = {}, glm::vec2 depth = {0.0f, 1.0f}) const noexcept;
	vk::Rect2D scissor(Viewport const& view = {}) const noexcept;

	Ref<Window> m_win;

  private:
	void updateStats();

	using Receiver = Ref<input::Receiver>;
	using TagDeque = TaggedDeque<Receiver, input::tag_t::type>;
	using Receivers = TaggedStore<Receiver, input::tag_t, TagDeque>;

	inline static Stats s_stats = {};

	io::Service m_io;
	std::optional<GFX> m_gfx;
	Editor m_editor;
	input::Driver m_input;
	struct {
		struct {
			time::Point stamp{};
			Time_s elapsed{};
			u32 count;
		} frame;
	} m_stats;
	Receivers m_receivers;
	input::State m_inputState;
	Desktop* m_pDesktop = {};
};

struct Engine::CreateInfo {
	std::optional<io::Path> logFile = "log.txt";
	LibLogger::Verbosity verbosity = LibLogger::libVerbosity;
};

struct Engine::DrawFrame {
	Context::Frame frame;
	Ref<Engine> engine;

	DrawFrame(Engine& engine, Context::Frame&& frame) noexcept;
	DrawFrame(DrawFrame&&) noexcept;
	DrawFrame& operator=(DrawFrame&&) noexcept;
	~DrawFrame();

	graphics::CommandBuffer& cmd() noexcept;
};

// impl

inline graphics::CommandBuffer& Engine::DrawFrame::cmd() noexcept {
	return frame.primary;
}

inline Engine::Stats const& Engine::stats() noexcept {
	return s_stats;
}
inline bool Engine::editorActive() const noexcept {
	return m_editor.active();
}
inline bool Engine::editorEngaged() const noexcept {
	return m_editor.active() && Editor::s_engaged;
}
inline bool Engine::booted() const noexcept {
	return m_gfx.has_value();
}
inline Engine::GFX& Engine::gfx() {
	ENSURE(m_gfx.has_value(), "Not booted");
	return *m_gfx;
}
inline Engine::GFX const& Engine::gfx() const {
	ENSURE(m_gfx.has_value(), "Not booted");
	return *m_gfx;
}
inline input::State const& Engine::inputState() const noexcept {
	return m_inputState;
}
} // namespace le
