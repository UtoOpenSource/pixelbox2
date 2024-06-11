/*
 * This file is a part of Pixelbox - Infinite 2D sandbox game
 * Help manager
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
#include "imgui_md.h"

#include <filesystem>
#include <fstream>
namespace fs = std::filesystem;

namespace pb {

namespace screen {

static struct HelpManager {
	struct HelpTab {
		std::string name;
		bool is_closed = false;
		std::string content;
		ImGui::MarkdownTree tree;

		void draw() {
			if (ImGui::BeginTabItem(name.c_str())) {
				if (ImGui::BeginChild("child_md", ImGui::GetContentRegionAvail(), ImGuiChildFlags_Border, 0)) {
					tree.draw(name.c_str());
				}
				ImGui::EndChild();
				ImGui::EndTabItem();
			}
		}
	};
	public:
	std::vector<HelpTab> tabs;
} man;

static bool loaded = false;

static Register r2([](int) {
	if (!show_help_window) return;
	if (!loaded) {
		std::string path = "./assets/help";
		std::stringstream buffer;
		for (const auto & entry : fs::directory_iterator(path)) {
   		std::ifstream f(entry.path());
			buffer << f.rdbuf();
			auto& v = man.tabs.emplace_back(HelpManager::HelpTab{.name=entry.path().filename(), .is_closed=false, .content=buffer.str(), .tree={}});
			v = man.tabs.back();
			ImGui::MarkdownParser parser(v.tree, v.content, false);
			//parser.parse(); // yuppi
			buffer = {};
		}
		loaded=true;
	}

	if (ImGui::Begin("User Guide", &show_help_window, ImGuiWindowFlags_Modal)) {
		if (ImGui::BeginTabBar("tabs")) {
			if (ImGui::BeginTabItem("Controls")) {
				if (ImGui::BeginChild("child_md", ImGui::GetContentRegionAvail(), ImGuiChildFlags_Border, 0)) {
					ImGui::SeparatorText("USER GUIDE:");
					ImGui::ShowUserGuide();
					ImGui::SeparatorText("DEBUG SHORTCUTS :");
					ImGui::BulletText("F1  - Open this Help");
					ImGui::BulletText("F7  - Open ImGUI Demo");
					ImGui::BulletText("F8  - Show Profiler");
					ImGui::BulletText("F10 - Show FPS Overlay");
				}
				ImGui::EndChild();
				ImGui::EndTabItem();
			}

			for (auto& t : man.tabs) {
				t.draw();
			}
			ImGui::EndTabBar();
		}
	}
	ImGui::End();
});

};

};