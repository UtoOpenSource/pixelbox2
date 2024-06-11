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

struct FuncUnion {
	int prio;
	std::function<void(int)> func;

 public:
	FuncUnion(std::function<void(int)>&& f, int _prio) : prio(_prio), func(f) {}
	void operator()(int stat) const {
		if (func) func(stat);
	}
	auto operator<=>(const FuncUnion& src) const { return prio <=> src.prio; }
};

struct FuncList : public std::priority_queue<struct FuncUnion>{auto begin(){return c.cbegin();
}	 // namespace screen
auto end() { return c.cend(); }
}	 // namespace pb
*funclist = nullptr;

Register::Register(std::function<void(int)> f, int priority) {
	if (!funclist) funclist = new FuncList;
	funclist->emplace(std::move(f), priority);
}

void DrawAll() {
	for (const FuncUnion& u : *funclist) {
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
		// setup InGui backends
		ImGui_ImplSDL2_InitForOpenGL(pb::window::ptr, SDL_GL_GetCurrentContext());
		ImGui_ImplOpenGL3_Init();
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
				if (io.WantCaptureMouse && (e.type == SDL_MOUSEBUTTONDOWN || e.type == SDL_MOUSEBUTTONUP || e.type == SDL_MOUSEMOTION)) continue;

				// handle input
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

	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplSDL2_Shutdown();
	ImGui::DestroyContext();
	save_all();
	pb::window::close();
	pb::client_settings.close();
	LOG_INFO("DONE!");
	return 0;
}