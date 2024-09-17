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

#pragma once
#include <SDL2/SDL_keyboard.h>

#include <functional>

#include "SDL_events.h"
#include "settings.hpp"

namespace pb {

namespace window {
extern SDL_Window* ptr;
extern int width;
extern int height;
extern int swap_interval;

extern int init(int flags);
extern int close();
extern void clear();
extern int input(SDL_Event& e);	 // -1 - requested exit, 0 - no ecents to process, 1 - there are events to process
extern void flush();

extern bool set_swap_interval(int interval);
};	// namespace window

namespace screen {

/** new GUI suystem... heh
 * MAY BE CONSTUUCTED BEFORE IMGUI CREATION!!11
 * destructed when not needed anymore at all
 * you must do stuff about creating things/setting tings in activate()/deactivate() functions! not const/destr!
 */
class UIInstance {
 public:
	UIInstance() = default;
	virtual ~UIInstance() {}

	/** since we need IMGUI COntext to be alive to free it's stuff (AND opengl for images and other things),
	 * we will use create() function to call it WHEN we init app for the first time.
	 * destroy() will be called ONLY for succesfuillly inited instances!
	 *
	 * destroy is called BEFORE destruction of GL and IMGUI context! Your last chance to free VBO/Textures!
	 */
	virtual void create() {}
	virtual void destroy() {}

	/* use ImGui here! Setup and support your window here too! */
	virtual void operator()(int) = 0;
};

/* adds UI objexct to list to be initialized? and processed!
 * this object WILL BE FREED later! DO NOT PASS STATIC REFERENCES IN!
 */
bool register_ui(UIInstance* instance);

/**
 Register global root IMGUI draw callbacks for pixelbox GUI.
 In them we create and manipulate window/windows as we wish, independently.
 I was constantly tried to create some sort of global window management system... But it's not worth it.
 So... Yea...

 Please not split build system in independent libraries, or this wll broke (as well as doctests)
 DEPRECATED AND REMOVED
*/
// struct Register {
//	public:
//	Register(std::function<void(int)> f, int priority = 0);
//	~Register() = default;
// };

/**
 * this "destructor" is guaranteed to be called BEFORE destructon of the GL context.
 * It's intended use is to DESTROY ALL TEXTURES/VBO/EBO/etc. without hardcoding callbacks everywhere
 */
// struct RegisterDestructor {
//	public:
//	RegisterDestructor(std::function<void(void)> f, int priority = 0);
//	~RegisterDestructor() = default;
// };

/** Screen is a BACKGROUND stuff that happens after IMGui update + Draw
 * We accurately ignore all input preserved by ingui and draw stuff after imgui using DEPTH buffer.
 */
class Screen {
 public:
	Screen() = default;
	virtual ~Screen() = 0;

 public:
	virtual void redraw() {}					 // draw & update stuff
	virtual void input(SDL_Event&) {}	 // input events
 public:
	/** since we need OpenGL context to properly free VBO's and textures,
	 * we use theese functions that are called every time we change screen
	 *
	 * after deactivate we wll NOT use this screen anymore, so for dynamic screens it is a good place to call delete :) (or whatever)
	 * Most of the screens are static. Exception - the world view, because it has a lot of stuff to keep track of
	 */
	virtual void activate() {}
	virtual void deactivate() {}
	/** Excape handler
	 * Every time user presses X on main window or ESC, we will call this callback.
	 * If it returns 0, we will exit succesfully.
	 * Else we will not exit;
	 *
	 * Wery userful, for example, to let world save in other thread and wait for it, while keeping track of the Popup and bloakcing ESC input.
	 */
	virtual int exit_req() { return 0; }
};

/** change current background screen on new one
 * If it allocated dynamicly - YOU are responcible for managing it's memory
 * Recommended way - delete in deactivate() call
 *
 * Calling on same screen as we have already has no efect and returns false;
 * Because of memory management - WE MUST CALL IT WITH NULL AT THE END OF PROGRAM (we do)
 */
bool change(Screen*);

/** DON'T USE FUNCTIONS BELOW! :D */
Screen* _GetCurrent();

/** "draw" and process all screens and windows. do not use anywhere except one place where this function is already placed! */
void _DrawAll();

/** initializes all ui objects registered! */
void _InitAll();

/** frees all ui objects and screen!. CALL BEFORE DEATH OF GLContext AND IMGUI's */
void _FreeAll();
};	// namespace screen

extern SettingsManager client_settings;

};	// namespace pb