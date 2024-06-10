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
#include "galogen.h"
#include "screen.hpp"
#include "profiler.hpp"
#include "printf.h"

namespace pb {

namespace window {
	static bool ok = true;
	static SDL_GLContext gl_context;

	SDL_Window* ptr = nullptr;
	int width = 640;
	int height = 480;

	int  init(int flags);
	int  close();
	void clear();
	int  input(SDL_Event& e);
	void flush();
	bool set_swap_interval(int interval);
};

bool pb::window::set_swap_interval(int interval) {
	using pb::window::gl_context;
	SDL_GL_MakeCurrent(window::ptr, gl_context);
	return SDL_GL_SetSwapInterval(interval) >= 0;
}

int window::init(int flags) {
	using pb::window::gl_context;

	if (SDL_Init(SDL_INIT_VIDEO) < 0) {
		printf_("Can't init SDL2!");
		return -1;
  }

	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
	SDL_GL_SetAttribute( SDL_GL_CONTEXT_MAJOR_VERSION, 3 );
  SDL_GL_SetAttribute( SDL_GL_CONTEXT_MINOR_VERSION, 3 );
	SDL_GL_SetAttribute( SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);

	int window_flags = (flags ? flags : SDL_WINDOW_RESIZABLE) | SDL_WINDOW_OPENGL ;

	window::ptr = SDL_CreateWindow("SDL2 Window", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
		window::width, window::height, window_flags);

	if (!window::ptr) {
		printf_("Can't open window!");
		return -1;
	}

	SDL_SetWindowMinimumSize(window::ptr, 200, 200);

	gl_context = SDL_GL_CreateContext(window::ptr);
	if (!gl_context) {
		SDL_DestroyWindow(window::ptr);
		printf_("Cannot create SDL_GLContext!");
		return -1;
	}

	SDL_GL_MakeCurrent(window::ptr, gl_context);
	window::set_swap_interval(1);
	glViewport(0, 0, window::width, window::height);
	ok = true;

	// clear stuff
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	SDL_GL_SwapWindow(window::ptr); 
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	return 0;
}

int window::close() {
	if (!window::ok) return -1;
	SDL_GL_DeleteContext(window::gl_context);
	SDL_DestroyWindow(window::ptr);
	SDL_Quit();
	return 0;
}

// poll events
int window::input(SDL_Event& e) {
	//SDL_Event e;
	while( SDL_PollEvent( &e ) != 0 ) {
		if(e.type == SDL_QUIT) {
			return -1;
		}

		// handle some resize
		if (e.type == SDL_WINDOWEVENT && e.window.event == SDL_WINDOWEVENT_RESIZED) {
			int w = e.window.data1, h = e.window.data2;
			window::width = w; window::height = h;
			glViewport(0, 0, w, h);
		}

		return 1;
	}
	return 0;
}

void window::clear() {
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void window::flush() {

	{
		PROFILING_SCOPE("SDL_FLUSH");
		SDL_GL_SwapWindow(window::ptr);
	}

}

};