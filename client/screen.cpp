/*
 * This file is a part of Pixelbox - Infinite 2D sandbox game
 * Graphics And screen management Core
 * Copyright (C) 2024 UtoECat
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "screen.hpp"

#include <queue>
#include <vector>

#include "SDL_events.h"
#include "SDL_keycode.h"
#include "SDL_video.h"
#include "external/imgui.h"
#include "imgui.h"
#include "imgui_impl_opengl3.h"
#include "imgui_impl_sdl2.h"

namespace pb {

SettingsManager client_settings;

namespace screen {

template <typename... Args>
struct FuncUnion {
	int prio;
	std::function<void(Args...)> func;

 public:
	FuncUnion(std::function<void(Args...)>&& f, int _prio) : prio(_prio), func(f) {}
	void operator()(Args&&... args) const {
		if (func) func(std::forward<Args>(args)...);
	}
	auto operator<=>(const FuncUnion& src) const { return prio <=> src.prio; }
};

// specialization... i fuckin hate c++
template <>
struct FuncUnion<void> {
	int prio;
	std::function<void()> func;

 public:
	FuncUnion(std::function<void()>&& f, int _prio) : prio(_prio), func(f) {}
	void operator()() const {
		if (func) func();
	}
	auto operator<=>(const FuncUnion& src) const { return prio <=> src.prio; }
};

/**
 * DRAWING CALLBBACKS
 */
struct DrawList : public std::priority_queue<struct FuncUnion<int>>{auto begin(){return c.cbegin();
}	 // namespace screen
auto end() { return c.cend(); }
}	 // namespace pb
*funclist = nullptr;

Register::Register(std::function<void(int)> f, int priority) {
	if (!funclist) funclist = new DrawList;
	funclist->emplace(std::move(f), priority);
}

/**
 * DESTRUCTORS
 */
struct FreeList : public std::priority_queue<struct FuncUnion<void>>{auto begin(){return c.cbegin();
}
auto end() { return c.cend(); }
}
*freelist = nullptr;

RegisterDestructor::RegisterDestructor(std::function<void(void)> f, int priority) {
	if (!freelist) freelist = new FreeList;
	freelist->emplace(std::move(f), priority);
}

void DrawAll() {
	for (auto& u : *funclist) {
		u(0);
	}
}

bool show_demo_window = false;
bool show_help_window = true;

/**
 * IMGUI debug and welcome window
 */
static Register r([](int) {
	if (!show_demo_window) return;
	ImGui::ShowDemoWindow(&show_demo_window);
});

/** REAL screen system */
static Screen* curr_scr = nullptr;
bool change(Screen* scr) {
	if (scr == curr_scr) return false;
	if (curr_scr) curr_scr->deactivate();
	curr_scr = scr;
	if (curr_scr) curr_scr->activate();
	return true;
}

Screen::~Screen() {}
}
;
}
;

#include "profiler.hpp"

static bool need_handle_exit_cond() {							 // exit requested
	if (pb::screen::curr_scr) {											 // if have screen
		return pb::screen::curr_scr->exit_req() == 0;	 // exit if requiest handler returns 0
	} else {
		return true;	// stop app
	}
	return false;	 // do not exit app, continue drawing and etc.
}

// debug shortcuts
static void extra_keys(SDL_Event& e) {
	if (e.type == SDL_KEYDOWN) {
		if (e.key.keysym.sym == SDLK_F1) pb::screen::show_help_window = !pb::screen::show_help_window;
		if (e.key.keysym.sym == SDLK_F7) pb::screen::show_demo_window = !pb::screen::show_demo_window;
		if (e.key.keysym.sym == SDLK_F8) pb::screen::show_profiler = !pb::screen::show_profiler;
		if (e.key.keysym.sym == SDLK_F10) pb::screen::show_fps_overlay = !pb::screen::show_fps_overlay;
	}
}

static void load_all() {
	auto& m = pb::client_settings;
	m.get("window_width", pb::window::width);
	m.get("window_height", pb::window::height);
	m.get("window_swap_interval", pb::window::swap_interval);
}

static void save_all() {
	auto& m = pb::client_settings;
	m.set("window_width", pb::window::width);
	m.set("window_height", pb::window::height);
	m.set("window_swap_interval", pb::window::swap_interval);
}

namespace ImGui {
extern void loadExtraFonts();
};

static void apply_my_theme() {
ImVec4* colors = ImGui::GetStyle().Colors;
colors[ImGuiCol_Text]                   = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
colors[ImGuiCol_TextDisabled]           = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);
colors[ImGuiCol_WindowBg]               = ImVec4(0.18f, 0.18f, 0.18f, 0.78f);
colors[ImGuiCol_ChildBg]                = ImVec4(0.28f, 0.28f, 0.28f, 0.09f);
colors[ImGuiCol_PopupBg]                = ImVec4(0.31f, 0.31f, 0.31f, 1.00f);
colors[ImGuiCol_Border]                 = ImVec4(0.45f, 0.45f, 0.45f, 0.42f);
colors[ImGuiCol_BorderShadow]           = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
colors[ImGuiCol_FrameBg]                = ImVec4(0.24f, 0.24f, 0.24f, 1.00f);
colors[ImGuiCol_FrameBgHovered]         = ImVec4(0.25f, 0.25f, 0.25f, 0.77f);
colors[ImGuiCol_FrameBgActive]          = ImVec4(0.40f, 0.39f, 0.39f, 1.00f);
colors[ImGuiCol_TitleBg]                = ImVec4(0.15f, 0.15f, 0.15f, 0.44f);
colors[ImGuiCol_TitleBgActive]          = ImVec4(0.15f, 0.15f, 0.15f, 0.76f);
colors[ImGuiCol_TitleBgCollapsed]       = ImVec4(0.15f, 0.15f, 0.15f, 0.20f);
colors[ImGuiCol_MenuBarBg]              = ImVec4(0.19f, 0.19f, 0.19f, 1.00f);
colors[ImGuiCol_ScrollbarBg]            = ImVec4(0.16f, 0.16f, 0.16f, 1.00f);
colors[ImGuiCol_ScrollbarGrab]          = ImVec4(0.27f, 0.27f, 0.27f, 1.00f);
colors[ImGuiCol_ScrollbarGrabHovered]   = ImVec4(0.30f, 0.30f, 0.30f, 1.00f);
colors[ImGuiCol_ScrollbarGrabActive]    = ImVec4(0.59f, 0.59f, 0.59f, 1.00f);
colors[ImGuiCol_CheckMark]              = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
colors[ImGuiCol_SliderGrab]             = ImVec4(0.39f, 0.39f, 0.39f, 1.00f);
colors[ImGuiCol_SliderGrabActive]       = ImVec4(0.70f, 0.70f, 0.70f, 1.00f);
colors[ImGuiCol_Button]                 = ImVec4(1.00f, 1.00f, 1.00f, 0.12f);
colors[ImGuiCol_ButtonHovered]          = ImVec4(1.00f, 1.00f, 1.00f, 0.16f);
colors[ImGuiCol_ButtonActive]           = ImVec4(1.00f, 1.00f, 1.00f, 0.39f);
colors[ImGuiCol_Header]                 = ImVec4(0.31f, 0.31f, 0.31f, 1.00f);
colors[ImGuiCol_HeaderHovered]          = ImVec4(0.47f, 0.47f, 0.47f, 1.00f);
colors[ImGuiCol_HeaderActive]           = ImVec4(0.47f, 0.47f, 0.47f, 1.00f);
colors[ImGuiCol_Separator]              = ImVec4(0.26f, 0.26f, 0.26f, 1.00f);
colors[ImGuiCol_SeparatorHovered]       = ImVec4(0.39f, 0.39f, 0.39f, 1.00f);
colors[ImGuiCol_SeparatorActive]        = ImVec4(0.59f, 0.59f, 0.59f, 1.00f);
colors[ImGuiCol_ResizeGrip]             = ImVec4(1.00f, 1.00f, 1.00f, 0.25f);
colors[ImGuiCol_ResizeGripHovered]      = ImVec4(1.00f, 1.00f, 1.00f, 0.67f);
colors[ImGuiCol_ResizeGripActive]       = ImVec4(0.59f, 0.59f, 0.59f, 1.00f);
colors[ImGuiCol_TabHovered]             = ImVec4(0.35f, 0.35f, 0.35f, 1.00f);
colors[ImGuiCol_Tab]                    = ImVec4(0.36f, 0.36f, 0.36f, 0.40f);
colors[ImGuiCol_TabSelected]            = ImVec4(0.36f, 0.36f, 0.36f, 1.00f);
colors[ImGuiCol_TabSelectedOverline]    = ImVec4(0.76f, 0.76f, 0.76f, 1.00f);
colors[ImGuiCol_TabDimmed]              = ImVec4(0.09f, 0.09f, 0.09f, 1.00f);
colors[ImGuiCol_TabDimmedSelected]      = ImVec4(0.19f, 0.19f, 0.19f, 1.00f);
colors[ImGuiCol_TabDimmedSelectedOverline]  = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);
colors[ImGuiCol_DockingPreview]         = ImVec4(0.86f, 0.86f, 0.86f, 0.70f);
colors[ImGuiCol_DockingEmptyBg]         = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
colors[ImGuiCol_PlotLines]              = ImVec4(0.47f, 0.47f, 0.47f, 1.00f);
colors[ImGuiCol_PlotLinesHovered]       = ImVec4(1.00f, 0.73f, 0.34f, 1.00f);
colors[ImGuiCol_PlotHistogram]          = ImVec4(0.58f, 0.58f, 0.58f, 1.00f);
colors[ImGuiCol_PlotHistogramHovered]   = ImVec4(0.94f, 0.60f, 0.39f, 1.00f);
colors[ImGuiCol_TableHeaderBg]          = ImVec4(0.19f, 0.19f, 0.20f, 1.00f);
colors[ImGuiCol_TableBorderStrong]      = ImVec4(0.41f, 0.41f, 0.43f, 1.00f);
colors[ImGuiCol_TableBorderLight]       = ImVec4(0.47f, 0.47f, 0.51f, 1.00f);
colors[ImGuiCol_TableRowBg]             = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
colors[ImGuiCol_TableRowBgAlt]          = ImVec4(0.45f, 0.41f, 0.41f, 0.06f);
colors[ImGuiCol_TextSelectedBg]         = ImVec4(0.57f, 0.53f, 0.53f, 0.16f);
colors[ImGuiCol_DragDropTarget]         = ImVec4(0.85f, 0.85f, 0.85f, 1.00f);
colors[ImGuiCol_NavHighlight]           = ImVec4(0.85f, 0.85f, 0.85f, 1.00f);
colors[ImGuiCol_NavWindowingHighlight]  = ImVec4(0.89f, 0.71f, 0.59f, 1.00f);
colors[ImGuiCol_NavWindowingDimBg]      = ImVec4(0.00f, 0.00f, 0.00f, 0.59f);
colors[ImGuiCol_ModalWindowDimBg]       = ImVec4(0.00f, 0.00f, 0.00f, 0.59f);
}

int main() {
	auto ctx = pb::prof::init_thread_data();

	{
		PROFILING_SCOPE("Init::ALL");
		pb::client_settings.open("client.db");	// settings
		load_all();

		if (pb::window::init(0) != 0) {
			return -1;
		}

		pb::client_settings.db.assert_owned();

		// Setup Dear ImGui context
		ImGui::CreateContext();
		ImGui::loadExtraFonts();
		ImGuiIO& io = ImGui::GetIO();
		io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;	 // Enable Keyboard Controls
		io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;	 // Enable Gamepad Controls
		io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;			 // Enable DOCKING!
		// setup InGui backends
		ImGui_ImplSDL2_InitForOpenGL(pb::window::ptr, SDL_GL_GetCurrentContext());
		ImGui_ImplOpenGL3_Init();
		apply_my_theme();
	}

	pb::client_settings.db.assert_owned();

	while (true) {
		{
			PROFILING_SCOPE("Render::Clear");
			pb::window::clear();
			// begin IMGUI
			ImGui_ImplOpenGL3_NewFrame();
			ImGui_ImplSDL2_NewFrame();
			ImGui::NewFrame();
		}

		{
			PROFILING_SCOPE("SDL::HandleInput");
			SDL_Event e = {};
			int status = 1;
			while ((status = pb::window::input(e)) > 0) {
				auto& io = ImGui::GetIO();
				ImGui_ImplSDL2_ProcessEvent(&e);
				extra_keys(e);

				// check if imgui want to exclusively capture theese events
				// ImGui already recieved events above in w.tick()!
				if (io.WantCaptureKeyboard && (e.type == SDL_KEYDOWN || e.type == SDL_KEYUP)) continue;
				if (io.WantCaptureMouse &&
						(e.type == SDL_MOUSEBUTTONDOWN || e.type == SDL_MOUSEBUTTONUP || e.type == SDL_MOUSEMOTION || e.type == SDL_MOUSEWHEEL))
					continue;

				// else handle input
				if (pb::screen::curr_scr) pb::screen::curr_scr->input(e);
			}

			if (status == -1 && need_handle_exit_cond()) {	// program exit condition
				break;																				// break infinite loop
			}
		}

		{
			PROFILING_SCOPE("ImGui::AllWindows")
			pb::screen::DrawAll();	// draw ingui windows
		}

		{
			PROFILING_SCOPE("Render::Screen")
			if (pb::screen::curr_scr) pb::screen::curr_scr->redraw();
		}

		{
			PROFILING_SCOPE("Render::ImGui")
			ImGui::Render();
			ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
		}

		{
			PROFILING_SCOPE("Render::Flush")
			pb::window::flush();
		}

		// step profiler in this thread
		ctx.step();
		pb::client_settings.db.assert_owned();
	}

	// finalization
	pb::screen::change(nullptr);	// free any current screen
	delete pb::screen::funclist;

	// free all. CALL "DESTRUCTORS" BEFORE DESTROYING GL CONTEXT!
	for (auto& a : *pb::screen::freelist) {
		a();
	}
	delete pb::screen::freelist;

	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplSDL2_Shutdown();
	ImGui::DestroyContext();
	save_all();
	pb::window::close();
	pb::client_settings.close();
	LOG_INFO("DONE!");
	return 0;
}