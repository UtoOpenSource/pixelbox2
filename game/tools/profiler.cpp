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

#include "base/profiler.hpp"

#include <algorithm>
#include <base/base.hpp>
#include <exception>
#include <iostream>
#include <sstream>
#include <vector>
#include <set>

#include "back_sdl/imgui.h"

namespace pb {

namespace tool {

using ZonePair = std::pair<HString, prof::prof_stats>;

struct compare_zones {
	bool operator()(const ZonePair& a, const ZonePair& b) const {
		float c = (a.second.owntime / (float)a.second.ncalls)*1000000;
		float d = (b.second.owntime / (float)b.second.ncalls)*1000000;
		float e = (std::hash<HString>{}(a.first)&0xFFFF)/(float)999999;
		float f = (std::hash<HString>{}(b.first)&0xFFFF)/(float)999999;
		return (c+e) > (d+f);
	}
};

using ZoneSet = std::set<std::pair<HString, prof::prof_stats>, compare_zones>;

ImColor get_str_color(const HString& s) {
	size_t hash = std::hash<HString>{}(s);
	const ImVec4 col = ImVec4(((hash&127)+127)/255.0, (((hash>>8)&127)+127)/255.0, (((hash>>16)&127)+127)/255.0, 1.0);
	return col;
}

static void add_calltable_row(const ZoneSet& zones) {
	for (auto &z : zones) {
		ImGui::PushID(std::hash<HString>{}(z.first));

		// Text and Tree nodes are less high than framed widgets, using AlignTextToFramePadding() we add vertical spacing to make the tree lines
		// equal high.
		ImGui::TableNextRow();
		ImGui::TableSetColumnIndex(0);
		ImGui::AlignTextToFramePadding();
		ImGui::TextColored(get_str_color(z.first), "%s", z.first.c_str());
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

static struct {
	// flags
	bool pause = false;
	bool force_short_stats = false;
	bool need_refresh = false;
	bool short_stats = false;
	// threads
	std::vector<prof::ThreadID> threrads;
	// history
	size_t history_pos = 0;
	std::vector<prof::StatsStorage> data = std::vector<prof::StatsStorage>(prof::history_size());	 // full stats
	// most heavier zones (short stats)
	std::set<std::pair<HString, prof::prof_stats>, compare_zones> zones;
} curr;

static void refresh_data(prof::ThreadID current_thread) {
	if (curr.pause && !curr.need_refresh) return;
	curr.need_refresh = false;

	curr.threrads = prof::get_threads();
	curr.zones.clear();

	if (curr.short_stats) {	 // short
		curr.data[curr.history_pos] = prof::get_summary(current_thread, curr.history_pos);
	} else {
		// full
		for (size_t i = 0; i < prof::history_size(); i++) {
			curr.data[i] = prof::get_summary(current_thread, i);
		}
	}

	// sort
	for (auto &p : curr.data[curr.history_pos]) {curr.zones.emplace(p);}
}

static void call_plotter() {
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

		for (auto item : curr.data[i]) {
			float item_v = (item.second.owntime / max_v) * range;
			draw_list->AddRectFilled(ImVec2(p.x, p.y), ImVec2(p.x + per_item, p.y + item_v), get_str_color(item.first));
			p.y += item_v;
		}
	}
}

void ProfilerWindow(bool *p_open) {
	static prof::ThreadID current_thread;
	auto prof = prof::get_thread_data();

	// Main body of the window
	if (!ImGui::Begin("Pofiler", p_open)) {
		// Early out if the window is collapsed, as an optimization.
		ImGui::End();
		return;
	}

	if (ImGui::Button(curr.pause ? "Resume" : "Pause")) {
		curr.pause = !curr.pause;
	}
	ImGui::SameLine();

	if (!curr.pause) ImGui::BeginDisabled();
	if (ImGui::Button("Refresh Once")) {
		curr.need_refresh = true;
	}
	if (!curr.pause) ImGui::EndDisabled();
	ImGui::SameLine();

	if (ImGui::Button((std::string("Force short stats :") + (curr.force_short_stats? "[+]" : "[-]")).c_str())) {
		curr.force_short_stats = !curr.force_short_stats;
	}

	// get data
	{
		PROFILING_SCOPE("Prof_refresh_data", prof);
	refresh_data(current_thread);
	}

	// draw zones
	static HString selected;
	{
		ImGui::BeginChild("threads_panel", ImVec2(150, 0), ImGuiChildFlags_Border | ImGuiChildFlags_ResizeX);
		for (auto v : curr.threrads) {
			std::string str("Thread ");
			str += id_to_string(v);
			if (ImGui::Selectable(str.c_str(), v == current_thread)) current_thread = v;
		}
		ImGui::EndChild();
	}
	ImGui::SameLine();

	// draw calls
	{
		ImGui::BeginGroup();
		ImGui::BeginChild("info_panel", ImVec2(0,
																					 -ImGui::GetFrameHeightWithSpacing()));	 // Leave room for
																																									 // 1 line below us
		std::string name = id_to_string(current_thread);
		ImGui::Text("Thread: %s\t\t(%i active zones)", name.c_str(), (int)curr.zones.size());

		ImGui::Separator();
		if (ImGui::BeginTabBar("##Tabs", ImGuiTabBarFlags_None)) {
			if (ImGui::BeginTabItem("Calls List")) {
				curr.short_stats = true;
				if (ImGui::BeginTable("##split", 4, ImGuiTableFlags_BordersOuter | ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY)) {
					ImGui::TableSetupScrollFreeze(0, 1);
					ImGui::TableSetupColumn("Zone name");
					ImGui::TableSetupColumn("Total time");
					ImGui::TableSetupColumn("Own time");
					ImGui::TableSetupColumn("Call count");
					ImGui::TableHeadersRow();

					// Iterate placeholder objects (all the same data)
					add_calltable_row(curr.zones);

					ImGui::EndTable();
				}
				ImGui::EndTabItem();
			}
			if (ImGui::BeginTabItem("Plotter")) {
				if (curr.force_short_stats) curr.short_stats = true;
				else curr.short_stats = false;
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

};	// namespace tool

};	// namespace pb