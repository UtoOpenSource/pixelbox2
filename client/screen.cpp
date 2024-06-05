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
#include "imgui.h"
#include <queue>
#include <vector>

namespace pb {

namespace screen {

	struct FuncUnion {
		int prio;
		std::function<void(int)> func;
		public:
		FuncUnion(std::function<void(int)>&& f, int _prio) : prio(_prio), func(f) {}
		void operator()(int stat) const {if (func) func(stat);}
		auto operator<=>(const FuncUnion& src) const {
			return prio<=>src.prio;
		}
	};

	struct FuncList : public std::priority_queue<struct FuncUnion> {
		auto begin() {return c.cbegin();}
		auto end() {return c.cend();}
	} *funclist = nullptr;

	Register::Register(std::function<void(int)> f, int priority) {
		if (!funclist) funclist = new FuncList;
		funclist->emplace(std::move(f), priority);
	}

	void DrawAll() {
		for(const FuncUnion& u : *funclist) {
			u(0);
		}
	}

	bool show_demo_window = true;

	/**
	 * IMGUI debug and welcome window
	 */
	static Register r([](int){
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

};

};

#include "profiler.hpp"

static bool handle_exit_cond(pb::BackendRAII &w) {
	if (!w.tick()) { // exit requested
		if (pb::screen::curr_scr) { // if have screen
			return pb::screen::curr_scr->exit_req() != 0; // not return if requiest handler returns 0
		} else {
			return false; // stop app
		}
	}
	return true; // do not exit app, continue drawing and etc.
}

int main() {

	auto ctx = pb::prof::init_thread_data();
	pb::BackendRAII &w = *pb::graphics;

	while (handle_exit_cond(w)) {
		{ 
			PROFILING_SCOPE("graphics->clear");
			w.clear();
		}

		{
			PROFILING_SCOPE("update")
			pb::screen::DrawAll();
		}

		ctx.step();

		{
			PROFILING_SCOPE("background->draw")
			if (pb::screen::curr_scr) pb::screen::curr_scr->redraw();
		}

		{
			PROFILING_SCOPE("dude->draw")
			
		}

		{
			PROFILING_SCOPE("grpahics->flush")
			w.flush();
		}
	}

	pb::screen::change(nullptr); // free any current screen
	delete pb::screen::funclist;
}