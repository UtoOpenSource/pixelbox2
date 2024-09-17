#include "profiler.hpp"
#include "screen.hpp"
#include "external/imgui.h"

#include "SDL_events.h"
#include "SDL_keycode.h"
#include "SDL_video.h"
#include "imgui_impl_opengl3.h"
#include "imgui_impl_sdl2.h"

#include "_ui_list.hpp"

static bool need_handle_exit_cond() {							 // exit requested
	if (pb::screen::_GetCurrent()) {											 // if have screen
		return pb::screen::_GetCurrent()->exit_req() == 0;	 // exit if requiest handler returns 0
	} else {
		return true;	// stop app
	}
	return false;	 // do not exit app, continue drawing and etc.
}

// debug shortcuts
static void extra_keys(SDL_Event& e) {
	if (e.type == SDL_KEYDOWN) {
        using namespace pb::screen::ui;
		if (e.key.keysym.sym == SDLK_F1) show_help_window = !show_help_window;
		if (e.key.keysym.sym == SDLK_F7) show_demo_window = !show_demo_window;
		if (e.key.keysym.sym == SDLK_F8) show_profiler = !show_profiler;
		if (e.key.keysym.sym == SDLK_F10) show_fps_overlay = !show_fps_overlay;
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
	colors[ImGuiCol_Text] = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
	colors[ImGuiCol_TextDisabled] = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);
	colors[ImGuiCol_WindowBg] = ImVec4(0.18f, 0.18f, 0.18f, 0.78f);
	colors[ImGuiCol_ChildBg] = ImVec4(0.28f, 0.28f, 0.28f, 0.09f);
	colors[ImGuiCol_PopupBg] = ImVec4(0.31f, 0.31f, 0.31f, 1.00f);
	colors[ImGuiCol_Border] = ImVec4(0.45f, 0.45f, 0.45f, 0.42f);
	colors[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
	colors[ImGuiCol_FrameBg] = ImVec4(0.24f, 0.24f, 0.24f, 1.00f);
	colors[ImGuiCol_FrameBgHovered] = ImVec4(0.25f, 0.25f, 0.25f, 0.77f);
	colors[ImGuiCol_FrameBgActive] = ImVec4(0.40f, 0.39f, 0.39f, 1.00f);
	colors[ImGuiCol_TitleBg] = ImVec4(0.15f, 0.15f, 0.15f, 0.44f);
	colors[ImGuiCol_TitleBgActive] = ImVec4(0.15f, 0.15f, 0.15f, 0.76f);
	colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.15f, 0.15f, 0.15f, 0.20f);
	colors[ImGuiCol_MenuBarBg] = ImVec4(0.19f, 0.19f, 0.19f, 1.00f);
	colors[ImGuiCol_ScrollbarBg] = ImVec4(0.16f, 0.16f, 0.16f, 1.00f);
	colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.27f, 0.27f, 0.27f, 1.00f);
	colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.30f, 0.30f, 0.30f, 1.00f);
	colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.59f, 0.59f, 0.59f, 1.00f);
	colors[ImGuiCol_CheckMark] = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
	colors[ImGuiCol_SliderGrab] = ImVec4(0.39f, 0.39f, 0.39f, 1.00f);
	colors[ImGuiCol_SliderGrabActive] = ImVec4(0.70f, 0.70f, 0.70f, 1.00f);
	colors[ImGuiCol_Button] = ImVec4(1.00f, 1.00f, 1.00f, 0.12f);
	colors[ImGuiCol_ButtonHovered] = ImVec4(1.00f, 1.00f, 1.00f, 0.16f);
	colors[ImGuiCol_ButtonActive] = ImVec4(1.00f, 1.00f, 1.00f, 0.39f);
	colors[ImGuiCol_Header] = ImVec4(0.31f, 0.31f, 0.31f, 1.00f);
	colors[ImGuiCol_HeaderHovered] = ImVec4(0.47f, 0.47f, 0.47f, 1.00f);
	colors[ImGuiCol_HeaderActive] = ImVec4(0.47f, 0.47f, 0.47f, 1.00f);
	colors[ImGuiCol_Separator] = ImVec4(0.26f, 0.26f, 0.26f, 1.00f);
	colors[ImGuiCol_SeparatorHovered] = ImVec4(0.39f, 0.39f, 0.39f, 1.00f);
	colors[ImGuiCol_SeparatorActive] = ImVec4(0.59f, 0.59f, 0.59f, 1.00f);
	colors[ImGuiCol_ResizeGrip] = ImVec4(1.00f, 1.00f, 1.00f, 0.25f);
	colors[ImGuiCol_ResizeGripHovered] = ImVec4(1.00f, 1.00f, 1.00f, 0.67f);
	colors[ImGuiCol_ResizeGripActive] = ImVec4(0.59f, 0.59f, 0.59f, 1.00f);
	colors[ImGuiCol_TabHovered] = ImVec4(0.35f, 0.35f, 0.35f, 1.00f);
	colors[ImGuiCol_Tab] = ImVec4(0.36f, 0.36f, 0.36f, 0.40f);
	colors[ImGuiCol_TabSelected] = ImVec4(0.36f, 0.36f, 0.36f, 1.00f);
	colors[ImGuiCol_TabSelectedOverline] = ImVec4(0.76f, 0.76f, 0.76f, 1.00f);
	colors[ImGuiCol_TabDimmed] = ImVec4(0.09f, 0.09f, 0.09f, 1.00f);
	colors[ImGuiCol_TabDimmedSelected] = ImVec4(0.19f, 0.19f, 0.19f, 1.00f);
	colors[ImGuiCol_TabDimmedSelectedOverline] = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);
	colors[ImGuiCol_DockingPreview] = ImVec4(0.86f, 0.86f, 0.86f, 0.70f);
	colors[ImGuiCol_DockingEmptyBg] = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
	colors[ImGuiCol_PlotLines] = ImVec4(0.47f, 0.47f, 0.47f, 1.00f);
	colors[ImGuiCol_PlotLinesHovered] = ImVec4(1.00f, 0.73f, 0.34f, 1.00f);
	colors[ImGuiCol_PlotHistogram] = ImVec4(0.58f, 0.58f, 0.58f, 1.00f);
	colors[ImGuiCol_PlotHistogramHovered] = ImVec4(0.94f, 0.60f, 0.39f, 1.00f);
	colors[ImGuiCol_TableHeaderBg] = ImVec4(0.19f, 0.19f, 0.20f, 1.00f);
	colors[ImGuiCol_TableBorderStrong] = ImVec4(0.41f, 0.41f, 0.43f, 1.00f);
	colors[ImGuiCol_TableBorderLight] = ImVec4(0.47f, 0.47f, 0.51f, 1.00f);
	colors[ImGuiCol_TableRowBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
	colors[ImGuiCol_TableRowBgAlt] = ImVec4(0.45f, 0.41f, 0.41f, 0.06f);
	colors[ImGuiCol_TextSelectedBg] = ImVec4(0.57f, 0.53f, 0.53f, 0.16f);
	colors[ImGuiCol_DragDropTarget] = ImVec4(0.85f, 0.85f, 0.85f, 1.00f);
	colors[ImGuiCol_NavHighlight] = ImVec4(0.85f, 0.85f, 0.85f, 1.00f);
	colors[ImGuiCol_NavWindowingHighlight] = ImVec4(0.89f, 0.71f, 0.59f, 1.00f);
	colors[ImGuiCol_NavWindowingDimBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.59f);
	colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.59f);
}

namespace pb {
extern void _init_client();
extern void _free_client();
};

int main() {
	auto ctx = pb::prof::init_thread_data();
	{
		PROFILING_SCOPE("Init::ALL");
        pb::_init_client();

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

        pb::screen::_InitAll(); // init all known UI's
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
				if (pb::screen::_GetCurrent()) pb::screen::_GetCurrent()->input(e);
			}

			if (status == -1 && need_handle_exit_cond()) {	// program exit condition
				break;																				// break infinite loop
			}
		}

		{
			//PROFILING_SCOPE("ImGui::Draw")
			pb::screen::_DrawAll();	// draw ingui windows
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
	pb::screen::_FreeAll(); // free all UI and screen stuff

	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplSDL2_Shutdown();
	ImGui::DestroyContext();
	save_all();
	pb::window::close();
	pb::client_settings.close();
    pb::_free_client();
	LOG_INFO("DONE!");
	return 0;
}