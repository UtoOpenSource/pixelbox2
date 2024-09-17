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

#include <vector>
#include "base.hpp"
#include "external/imgui.h"
#include "imgui.h"
#include "profiler.hpp"

namespace pb {

SettingsManager client_settings;

namespace screen {

/**
 * LIST OF OUR BOIIS!!!
 */
 struct UIManager : public std::vector<screen::UIInstance*> {
		using C = std::vector<screen::UIInstance*>;
		using C::C;
		auto begin() { return this->cbegin(); }
		auto end() { return this->cend(); }
	};

static UIManager* __uiman = nullptr;
static UIManager* get_ui_manager() {
	if (!__uiman) {
		__uiman = new UIManager();
	}
	return __uiman;
};

static bool is_init = false;

/* adds UI objexct to list to be initialized? and processed! */
bool register_ui(UIInstance* instance) {
	if (!instance) return false;
	auto* man = get_ui_manager();

	for (auto* other : *man) {
		// should not be reached! just fool safety
		if (other == instance) return false;
		// yeah... fuck this! :)
	}

	if (is_init) instance->create();
	man->push_back(instance);
	return true;
}

/**
 * IMGUI debug and welcome window
 */
namespace ui {
	bool show_demo_window = false;
	static class demo_window : public UIInstance {
		void operator()(int) override {
			if (!show_demo_window) return;
			ImGui::ShowDemoWindow(&show_demo_window);
		}
	} v;
	UIInstance* demo_window = &v; 
};

/** REAL screen system */
static Screen* curr_scr = nullptr;

bool change(Screen* scr) {
	if (scr == curr_scr) return false;
	if (curr_scr) curr_scr->deactivate();
	curr_scr = scr;
	if (curr_scr) curr_scr->activate();
	return true;
}

Screen* _GetCurrent() {
	return curr_scr;
}

/** initializes all ui objects registered! */
void _InitAll() {
	if (is_init) return;
	auto* man = get_ui_manager();
	auto it = man->begin();

	for (; it != man->end();) {
		auto* inst = *it;
		try {
			inst->create();
			it++;
		} catch (std::exception& e) {
			LOG_ERROR("UIInstance creation ERROR : %s!", e.what());
			it = man->erase(it); // fuck u! :D
			//delete inst;
		}
	}
	is_init = true;
}

/** frees all ui objects and screen!. CALL BEFORE DEATH OF GLContext AND IMGUI's */
void _FreeAll() {
	assert(is_init == true);
	pb::screen::change(nullptr);	// free any current screen

	auto* man = get_ui_manager();
	for (auto* inst : *man) {
		try {
			inst->destroy();
		} catch (std::exception& e) {
			LOG_ERROR("UIInstance finalization ERROR : %s!", e.what());
		}
	}

	// foolprof shinanigans
	for (auto* inst : *man) {
		// nothing
	}

	// delete our man... :(
	delete __uiman;
	is_init = false;
}

void _DrawAll() {
	{
		PROFILING_SCOPE("Render::UserInterface")
		auto* man = get_ui_manager();
		for (auto* inst : *man) {
			(*inst)(0); // no try/catch shenanigans for perfomance reasons
		}
	}

	{
		PROFILING_SCOPE("Render::Screen")
		if (pb::screen::curr_scr) pb::screen::curr_scr->redraw();
	}
}

Screen::~Screen() {}
};	// namespace screen
};	// namespace pb
