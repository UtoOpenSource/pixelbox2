/*
 * This file is a part of Pixelbox - Infinite 2D sandbox game
 * Profiler renderer
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

#define PROFILER_DEFINE_EXT
#include "profiler.hpp"

#include <engine/base.hpp>
#include <sstream>
#include <vector>
#include <set>

#include "imgui.h"

#include "screen.hpp"

namespace pb {

namespace screen {
	bool show_profiler = true;
};

namespace tools {

using ZonePair = std::pair<const std::string*, prof::prof_stats>;

struct compare_zones {
	bool operator()(const ZonePair& a, const ZonePair& b) const {
		float c = (a.second.owntime / (float)a.second.ncalls);
		float d = (b.second.owntime / (float)b.second.ncalls);
		return (c) > (d);
	}
};

using ZoneSet = std::multiset<std::pair<const std::string*, prof::prof_stats>, compare_zones>;

/** utility  functions*/
static ImColor get_str_color(const std::string& s) {
	size_t hash = std::hash<std::string>{}(s);
	const ImVec4 col = ImVec4(((hash&127)+127)/255.0, (((hash>>8)&127)+127)/255.0, (((hash>>16)&127)+127)/255.0, 1.0);
	return col;
}

static void add_calltable_row(const ZoneSet& zones) {
	for (auto &z : zones) {
		ImGui::PushID(std::hash<const std::string*>{}(z.first));

		// Text and Tree nodes are less high than framed widgets, using AlignTextToFramePadding() we add vertical spacing to make the tree lines
		// equal high.
		ImGui::TableNextRow();
		ImGui::TableSetColumnIndex(0);
		ImGui::AlignTextToFramePadding();
		ImGui::TextColored(get_str_color(*z.first), "%s", z.first->c_str());
		ImGui::TableSetColumnIndex(1);
		ImGui::Text("%f", z.second.sumtime);
		ImGui::TableSetColumnIndex(2);
		ImGui::Text("%f", z.second.owntime);
		ImGui::TableSetColumnIndex(3);
		ImGui::Text("%i", z.second.ncalls);

		ImGui::PopID();
	}
}

static std::string id_to_string(prof::ThreadID id) {
	std::stringstream strm;
	strm << id;
	return strm.str();
}

/** Implementation */
class Profiler {
	bool pause;
	bool force_short_stats = false;
	bool need_refresh = false;
	bool short_stats = false;
	// threads
	prof::ThreadID current_thread;
	std::vector<prof::ThreadID> threrads;
	// history
	size_t history_pos = 0;
	std::vector<prof::StatsStorage2> data = std::vector<prof::StatsStorage2>(prof::history_size());	 // full stats
	// most heavier zones (short stats)
	ZoneSet zones;
	private:
	void refresh_data(prof::ThreadData& prof);
	void call_plotter();
	public:
	~Profiler() {}
	void operator()();
};

void Profiler::refresh_data(prof::ThreadData& prof) {
	if (pause && !need_refresh) return;
	need_refresh = false;

	prof::get_threads(threrads);
	zones.clear();

	if (short_stats || force_short_stats) {	 // short
		data[history_pos] = prof::get_summary(current_thread, history_pos);
	} else {
		// full
		PROFILING_SCOPE_X("Prof_refresh_data_long", prof);
		size_t i = history_pos;
		size_t dst = prof::get_current_position(current_thread);
		size_t size = prof::history_size();
		if (dst >= size) throw "wat";
		while (i != dst) {
			data[i] = prof::get_summary(current_thread, i);
			i++;
			if (i >= size) i = 0;
		}
		data[dst] = prof::get_summary(current_thread, i);
		history_pos = i;
	}

	// sort
	for (auto &p : data[history_pos]) {zones.emplace(p);}
}

void Profiler::call_plotter() {
	ImDrawList *draw_list = ImGui::GetWindowDrawList();
	const ImVec2 pos = ImGui::GetCursorScreenPos();
	const auto scr = ImGui::GetContentRegionAvail();

	float per_item = scr.x / (float)prof::history_size();
	//const ImVec4 col = ImVec4(1.0f, 1.0f, 0.4f, 1.0f);
	const float max_v = 1 / 60.0;

	for (int i = 0; i < (int)prof::history_size(); i++) {
		ImVec2 p = pos;
		p.x += i * per_item;
		const float range = scr.y - 5;
		//p.y = range;

		for (auto item : data[i]) {
			float item_v = (item.second.owntime / max_v) * range;
			draw_list->AddRectFilled(ImVec2(p.x, p.y), ImVec2(p.x + per_item, p.y + item_v), get_str_color(*item.first));
			p.y += item_v;
		}
	}
}

void Profiler::operator()() {
	auto prof = prof::get_thread_data();

	// Main body of the window
	if (!ImGui::Begin("Pofiler", &screen::show_profiler)) {
		// Early out if the window is collapsed, as an optimization.
		ImGui::End();
		return;
	}

	if (ImGui::Button(pause ? "Resume" : "Pause")) {
		pause = !pause;
	}
	ImGui::SameLine();

	if (!pause) ImGui::BeginDisabled();
	if (ImGui::Button("Refresh Once")) {
		need_refresh = true;
	}
	if (!pause) ImGui::EndDisabled();
	ImGui::SameLine();

	if (ImGui::Button((std::string("Force short stats :") + (force_short_stats? "[+]" : "[-]")).c_str())) {
		force_short_stats = !force_short_stats;
	}

	// get data
	refresh_data(prof);

	// draw threads
	{
		ImGui::BeginChild("threads_panel", ImVec2(150, 0), ImGuiChildFlags_Border | ImGuiChildFlags_ResizeX);
		for (auto v : threrads) {
			std::string str("Thread ");
			str += id_to_string(v);
			if (ImGui::Selectable(str.c_str(), v == current_thread)) {
				current_thread = v;
				need_refresh = true;
				history_pos = 0;
			}
		}
		ImGui::EndChild();
	}
	ImGui::SameLine();

	// draw zones
	{
		ImGui::BeginGroup();
		ImGui::BeginChild("info_panel", ImVec2(0,
																					 -ImGui::GetFrameHeightWithSpacing()));	 // Leave room for
																																									 // 1 line below us
		std::string name = id_to_string(current_thread);
		ImGui::Text("Thread: %s\t\t(%i active zones)", name.c_str(), (int)zones.size());

		ImGui::Separator();
		if (ImGui::BeginTabBar("##Tabs", ImGuiTabBarFlags_None)) {
			if (ImGui::BeginTabItem("Calls List")) {
				short_stats = true;
				if (ImGui::BeginTable("##split", 4, ImGuiTableFlags_BordersOuter | ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY)) {
					ImGui::TableSetupScrollFreeze(0, 1);
					ImGui::TableSetupColumn("Zone name");
					ImGui::TableSetupColumn("Total time");
					ImGui::TableSetupColumn("Own time");
					ImGui::TableSetupColumn("Call count");
					ImGui::TableHeadersRow();

					// Iterate placeholder objects (all the same data)
					add_calltable_row(zones);

					ImGui::EndTable();
				}
				ImGui::EndTabItem();
			}
			if (ImGui::BeginTabItem("Plotter")) {
				short_stats = false;
				ImGui::TextWrapped("Pause and see 'Call list' for associated colors");
				call_plotter();

				ImGui::EndTabItem();
			}
			ImGui::EndTabBar();
		}
		ImGui::EndChild();
		ImGui::EndGroup();
	}
	ImGui::End();
	return;
}

};	// namespace tools

namespace screen {

	static pb::tools::Profiler prof;
	static Register _a([](int){
		if (!screen::show_profiler) return;
		prof();
	});
};

};	// namespace pb