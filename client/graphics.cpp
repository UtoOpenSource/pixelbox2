/*
 * This file is a part of Pixelbox - Infinite 2D sandbox game
 * Graphics IMGUI/SDL Setup Wrappers
 * Copyright (C) 2023-2024 UtoECat
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

#include <SDL2/SDL.h>
#include <SDL2/SDL_video.h>
#include <memory>
#include "external/imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_opengl3.h"
#include "galogen.h"

namespace pb {

	int window_width = 680;
	int window_height = 480;

namespace backend {

class SDLRAII {
	public:
	bool ok;
	SDL_GLContext gl_context;
	SDL_Window* window;
	SDLRAII();
	virtual ~SDLRAII();
	virtual void clear();
	virtual void flush();
	virtual bool tick();
};

SDLRAII::SDLRAII() {
	// Setup Dear ImGui context
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

	if (SDL_Init(SDL_INIT_VIDEO) < 0) {
		ImGui::DestroyContext();
		std::runtime_error("Failed to initialize the SDL2 library\n");
  }

	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
	SDL_GL_SetAttribute( SDL_GL_CONTEXT_MAJOR_VERSION, 3 );
  SDL_GL_SetAttribute( SDL_GL_CONTEXT_MINOR_VERSION, 0 );

	int window_flags = SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE;

	window = SDL_CreateWindow(
		"SDL2 Window", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
		680, 480, window_flags);
	SDL_SetWindowMinimumSize(window, 200, 200);

	if (!window) {
		ImGui::DestroyContext();
		SDL_DestroyWindow(window);
		std::runtime_error("Can't open a window!");
	}

	gl_context = SDL_GL_CreateContext(window);
	if (!gl_context) {
		ImGui::DestroyContext();
		SDL_GL_DeleteContext(gl_context);
		SDL_DestroyWindow(window);
		std::runtime_error("Cannot create SDL_GLContext!");
	}
	SDL_GL_MakeCurrent(window, gl_context);
	SDL_GL_SetSwapInterval(1);
	glViewport(0, 0, 200, 200);
	
	ImGui_ImplSDL2_InitForOpenGL(window, gl_context);
	ImGui_ImplOpenGL3_Init();
	ok = true;
}

SDLRAII::~SDLRAII() {
	if (!ok) return;
	ImGui_ImplOpenGL3_Shutdown();
	SDL_GL_DeleteContext(gl_context);
	ImGui_ImplSDL2_Shutdown();
	SDL_DestroyWindow(window);
	SDL_Quit();
	ImGui::DestroyContext();
}

std::unique_ptr<SDLRAII> init() {
	return std::make_unique<SDLRAII>();
}

// poll events
bool SDLRAII::tick() {
	SDL_Event e;
	while( SDL_PollEvent( &e ) != 0 ) {
		if(e.type == SDL_QUIT) {
			return false;
		}

		if (e.type == SDL_WINDOWEVENT && e.window.event == SDL_WINDOWEVENT_RESIZED) {
			int w = e.window.data1, h = e.window.data2;
			window_width = w; window_height = h;
			glViewport(0, 0, w, h);
		}

		ImGui_ImplSDL2_ProcessEvent(&e); // Forward your event to backend
	}
	return true;
}

void SDLRAII::clear() {
	ImGui_ImplOpenGL3_NewFrame();
	ImGui_ImplSDL2_NewFrame();
	ImGui::NewFrame();
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void SDLRAII::flush() {
	ImGui::Render();
	ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

	SDL_GL_SwapWindow(window);
}

};

};

#include "imgui.h"

int main() {
	pb::backend::SDLRAII w;
	while (w.tick()) {
		w.clear();
		ImGui::ShowDemoWindow();
		w.flush();
	}
}