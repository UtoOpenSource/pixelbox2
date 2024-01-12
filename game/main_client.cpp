/*
 * This file is a part of Pixelbox - Infinite 2D sandbox game
 * Client setup logic
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

#include <base/base.hpp>
#include <chrono>
#include <exception>
#include <iostream>
#include <ratio>
#include <thread>
#include <vector>
#include "back_sdl/imgui.h"
#include "base/profiler.hpp"
#include "back_sdl/graphics.hpp"

#include "tools/tools.hpp"

namespace pb {

	static void profiler_loadtest(int deep, prof::ThreadData& prof) {
		if (deep < 7) profiler_loadtest(deep+1, prof);
		for (int i = 1; i < 7; i++) {
			const HString v(std::string("_") + std::to_string(deep) + "_" + std::to_string(i + rand()%32));
			PROFILING_SCOPE(v, prof);
			if (rand() % 10 == 2) std::this_thread::sleep_for(std::chrono::nanoseconds(1));
		}
	}

	// used in main() function only anyway...
	void main_client(std::vector<const char*>) {
		auto prof = pb::prof::get_thread_data();
		std::unique_ptr<backend::BackendRAII> GL;

		// init
		{
			PROFILING_SCOPE("Initialize grpahics", prof);
			GL = backend::init();
		}

		static bool show_profiler = true;

		// loop
		while(GL->tick()) {
			{
			PROFILING_SCOPE("Clearup", prof);
			GL->clear();
			}

			{
			PROFILING_SCOPE("ImGui", prof);
			ImGui::ShowDemoWindow();

			tool::ProfilerWindow(&show_profiler);
			}

			{
			PROFILING_SCOPE("Game Logic", prof);
			//for (int i = 0; i < 10; i++)
			profiler_loadtest(0, prof);
			}

			{
			PROFILING_SCOPE("Flush framebuffer", prof);
			GL->flush();
			}
			prof.step();
		}
		// uninit is done automaticlu :p
	}
};