/*
 * This file is a part of Pixelbox - Infinite 2D sandbox game
 * Entry point and argument parser
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
#include <exception>
#include <iostream>
#include <type_traits>
#include <vector>
#include "base/profiler.hpp"
#include <base/doctest.h>
#include "external/enet.h"

/*
#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_opengl3.h" */

namespace pb {

	static constexpr int VERSION_MAJOR = 0;
	static constexpr int VERSION_MINOR = 8;

	void main_client(std::vector<const char*>); // main client routine
	void main_server(std::vector<const char*>); // main server routine

	bool is_client = true;

};

int parse_args(int argc, const char** argv, std::vector<const char*>& skipped) {
	for (int i = 1; i < argc; i++) {
		if (argv[i][0] == '-') {
			switch (argv[i][1]) {
				case 'v' :
					printf("ver = %1i.%1i\n", pb::VERSION_MAJOR, pb::VERSION_MINOR);
				break;
				case 't' : {
					printf("unit testing started up!\n");
					// unit-testing
					doctest::Context context(argc - i, argv + i);
					int res = context.run();
					if (context.shouldExit() || res) {
						printf("Test failed or user wants to stop\n");
						return -1;
					}
				}
				break;
				default: {
					skipped.push_back(argv[i]);
				}
				break;
			};
		} else skipped.push_back(argv[i]);
	}
	return 0;
}

int main(int argc, const char** argv) {
	enet_initialize();

	auto _td = pb::prof::make_thread_data();
	std::vector<const char*> skipped_argv;

	int v = parse_args(argc, argv, skipped_argv);
	if (v != 0) return v;

	try {
		if (pb::is_client)
			pb::main_client(std::move(skipped_argv));
		else pb::main_server(std::move(skipped_argv));
	} catch (std::exception& e) {
		std::cout << "Unhandled exception(std::exception)! : " << e.what() << std::endl;
	} catch (const char* e) {
		std::cout << "Unhandled exception(const char*)! : " << e << std::endl;
	} catch (...) {
		std::cout << "Unhandled exception of unknown type! : " << std::endl;
	}

	enet_deinitialize();
	return -1;
}
