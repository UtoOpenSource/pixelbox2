/*
 * This file is a part of Pixelbox - Infinite 2D sandbox game
 * Useful ImGUI Tools.
 * Copyright (C) 2023-2024 UtoECat
 *
 * This program is free software: you can redistribute it and/or
 * modify it under the terms of the GNU Affero General Public License
 * as published by the Free Software Foundation, either version 3 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public
 * License along with this program.  If not, see
 * <https://www.gnu.org/licenses/>.
 */

#include <memory>
#include "back_sdl/imgui.h"
#include "base/base.hpp"
#include "game/tools/tools.hpp"
#include "back_sdl/imgui_internal.h"

namespace pb {

void ToolManager::process() {
	for (auto &pair : map) {
		if (pair.second->is_shown) pair.second->operator()(*this);
	}
}

int ToolManager::count() { return (int)map.size(); }

/** SAVE/LOAD LAYOUT */
void ToolManager::save() {
	save_buffer = ImGui::SaveIniSettingsToMemory();
}
void ToolManager::load() {
	ImGui::LoadIniSettingsFromMemory(save_buffer.c_str(), (int) save_buffer.size());
}
std::string& ToolManager::get_saved() {
	return save_buffer;
}

bool ToolManager::is_opened(const HString& name) {
	auto v = map.find(name);
	if (v == map.end()) return false;
	return v->second->is_shown;
}

bool ToolManager::open(const HString& name) {
	auto v = map.find(name);
	if (v == map.end()) return false;
	if (v->second->is_shown) return false;
	v->second->is_shown = true;
	return true;
}

bool ToolManager::add_window(const HString& name, std::unique_ptr<Tool>&& src) {
	auto v = map.find(name);
	if (v != map.end()) return false;
	return map.emplace(name, std::move(src)).second;
}

bool ToolManager::close(const HString& name) {
	auto it = map.find(name);
	if (it == map.end()) return false;
	if (!it->second->is_shown) return false;
	it->second->is_shown = false;
	return true;
}

const std::map<const HString, std::unique_ptr<Tool>>& ToolManager::get_map() {
	return map;
}

Tool::~Tool() {}
void Tool::operator()(ToolManager&) {}

};	// namespace pb

namespace pb {

namespace tools {

class Master : public Tool {
	bool once = false;
	char search_buf[255] = "";
	HString selected_window;
	public:
	~Master() {}
	void operator()(ToolManager& manager) {
		if (!ImGui::Begin("Tools Master", NULL)) {
			ImGui::End();
			return;
		}

		if (!once) {
			ImGui::SetWindowCollapsed(true);
			once = true;
		}

		if (ImGui::BeginTabBar("master_tabs")) {

			if (ImGui::BeginTabItem("Execute")) {
				ImGui::TextWrapped("Here will be a list of all the tools you can run :p");
				ImGui::InputText("search", search_buf, 254);
				
				ImGui::BeginChild("items_panel", ImVec2(0, -ImGui::GetFrameHeightWithSpacing()), ImGuiChildFlags_Border);
				HString search (search_buf);

				for (auto &i : manager.get_map()) {
					if (i.first.find(search) == i.first.npos) continue;
					if (ImGui::Selectable(i.first.c_str(), i.first == selected_window))
						selected_window = i.first;
				}
				ImGui::EndChild();

				if (ImGui::Button("Open")) {
					manager.open(selected_window);
				}
				ImGui::EndTabItem();
			}

			
			if (ImGui::BeginTabItem("Window Manager")) {
				ImGui::TextWrapped("Here will be a list of all windows in the system");
				ImGui::BeginChild("left panel", ImVec2(0, -ImGui::GetFrameHeightWithSpacing()), ImGuiChildFlags_Border);
				for (auto &i : manager.get_map()) {
					if (ImGui::Selectable(i.first.c_str(), i.first == selected_window))
						selected_window = i.first;
				}

				if (ImGui::Button("On Top")) {
					//manager.forward(selected_window);
					auto *w = ImGui::FindWindowByName(selected_window.c_str());
					if (w) {
						printf("ok\n");
						ImGui::FocusWindow(w);
					}
				}
				ImGui::SameLine();
				if (ImGui::Button("Close")) {
					manager.close(selected_window);
				}

				ImGui::EndChild();
				ImGui::EndTabItem();
			}

		}
		ImGui::EndTabBar();

		ImGui::End();
	}
};

ToolConstructor CMaster() {
	return []() { return std::make_unique<Master>();};
}

};	// namespace tools

};	// namespace pb