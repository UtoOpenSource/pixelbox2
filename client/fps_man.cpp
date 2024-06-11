/*
 * This file is a part of Pixelbox - Infinite 2D sandbox game
 * FPS manager - record and show fps and frametime stats
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
#include <cstdio>
#include <deque>

namespace pb {

namespace screen {


	/**
	 * FPS Overlay window and FPS info system
	 */
	// set to true to show window!
	bool show_fps_overlay = false;

	struct frametime_summary {
		double avg = 0, min=0, max=0;
		int real_fps = 0;

		// calc using history of time directly
		void calc_history(std::deque<double> history, size_t start_index=1) {
			avg = 0, min=999999, max=0; // soft reset
			size_t real_count =0;
			if (history.size()-start_index > 0) { // calc values
				double old = history[0];
				for (size_t i = start_index; i < history.size(); i++) {
					real_count++;
					double time = history[i] - old; // frametime
					old = history[i];

					avg += time;
					if (min > time) min = time;
					if (max < time) max = time;
				}
				avg = avg / (double)(real_count); // average
			}
		}

		// calc from summary (and fps too!)
		void calc_summary(std::deque<frametime_summary> frametimes, size_t count = 5) {
			avg = 0, min=999999, max=0, real_fps=0;
			size_t real_count = 0;

			for (size_t i = 0; i < frametimes.size() && i < count; i++) {
				real_count++;
				frametime_summary& x = frametimes[i];
				avg += x.avg;
				real_fps += x.real_fps;
				if (min > x.min) min = x.min;
				if (max < x.max) max = x.max;
			}

			avg = avg / (double)(real_count); // average
			real_fps = real_fps / (double)(real_count); // average fps
		}
	};

	struct frametime_history {
		double old_time; // time since last shot
		double second_ago; // time since last second
		int fps_counter = 0;

		std::deque<double> history; // time shots. to calculate all this stuff. NEW ON BACK
		
		std::deque<frametime_summary> frametimes; // frametimes (min, max, avg) for every second. NEW ON FRONT

		frametime_summary sum_sec, sum_5sec, sum_15sec;

		void reset() {
			old_time = 0;
			history.clear();
			sum_sec = frametime_summary{};
			sum_5sec = frametime_summary{};
			sum_15sec = frametime_summary{};
			fps_counter = 0;
		}

		void add(double time, double story_size=1) {
			history.push_back(time);
			fps_counter++;

			//. older than 1 sec => remove
			while (!history.empty() && old_time - history.front() > story_size) {
				history.pop_front();
			}

			sum_sec.calc_history(history);

			if (time - second_ago >= 1) { // FPS
				sum_sec.real_fps = fps_counter;
				fps_counter = 0;
				frametimes.push_front(sum_sec); // hehe
				if (frametimes.size() > 15) frametimes.pop_back(); // remove old one
				sum_5sec.calc_summary(frametimes, 5);
				sum_15sec.calc_summary(frametimes, 15);
				second_ago = time;
			}

			old_time = time;
		}
	};

  static struct fps_info_man {
		frametime_history story;
		int count = 0;
		void collect();
		void draw();
	} fps_man;

	static Register FPS_OVERLAY([](int){
    static int location = 0;
		// collect fps info anyway
		fps_man.collect();
		if (!show_fps_overlay) return;

    ImGuiIO& io = ImGui::GetIO();
    ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav;
    if (location >= 0)
    {
        const float PAD = 10.0f;
        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImVec2 work_pos = viewport->WorkPos; // Use work area to avoid menu-bar/task-bar, if any!
        ImVec2 work_size = viewport->WorkSize;
        ImVec2 window_pos, window_pos_pivot;
        window_pos.x = (location & 1) ? (work_pos.x + work_size.x - PAD) : (work_pos.x + PAD);
        window_pos.y = (location & 2) ? (work_pos.y + work_size.y - PAD) : (work_pos.y + PAD);
        window_pos_pivot.x = (location & 1) ? 1.0f : 0.0f;
        window_pos_pivot.y = (location & 2) ? 1.0f : 0.0f;
        ImGui::SetNextWindowPos(window_pos, ImGuiCond_Always, window_pos_pivot);
        window_flags |= ImGuiWindowFlags_NoMove;
    }
    else if (location == -2)
    {
        // Center window
        ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
        window_flags |= ImGuiWindowFlags_NoMove;
    }
    ImGui::SetNextWindowBgAlpha(0.35f); // Transparent background
    if (ImGui::Begin("Fps overlay (now, 1 sec, 5 sec)", &show_fps_overlay, window_flags))
    {
				fps_man.draw();

        if (ImGui::BeginPopupContextWindow())
        {
            if (ImGui::MenuItem("Custom",       NULL, location == -1)) location = -1;
            if (ImGui::MenuItem("Center",       NULL, location == -2)) location = -2;
            if (ImGui::MenuItem("Top-left",     NULL, location == 0)) location = 0;
            if (ImGui::MenuItem("Top-right",    NULL, location == 1)) location = 1;
            if (ImGui::MenuItem("Bottom-left",  NULL, location == 2)) location = 2;
            if (ImGui::MenuItem("Bottom-right", NULL, location == 3)) location = 3;
            if (show_fps_overlay && ImGui::MenuItem("Close")) show_fps_overlay = false;
            ImGui::EndPopup();
        }
    }
    ImGui::End();
	});

};

};


#include "clock.hpp"
void pb::screen::fps_info_man::collect() {
	story.add(pb::__clocksource.time());
}

static void draw_summary(pb::screen::frametime_summary& s, bool alt=false, double period=1) {
	ImGui::TableNextRow();
	ImGui::TableSetColumnIndex(0);
	ImGui::Text("%1.0fs", period);
	ImGui::TableNextColumn();
	if (alt) ImGui::Text("%03i FPS (%03ims)", s.real_fps, int((1.0/double(s.real_fps)) * 1000));
	else ImGui::Text("%03i FPS", s.real_fps);
	
	#define COLV(v) ImGui::TableNextColumn(); if (alt) ImGui::Text("%03ims (%03i)", int(s.v*1000), s.v > 0 ? int(1.0/s.v) : 0); else ImGui::Text("%03ims", int(s.v*1000));
	COLV(min);
	COLV(avg);
	COLV(max);
	#undef COLV
}

void pb::screen::fps_info_man::draw() {
	static bool alternative_unit = false;
	ImGui::Text("Fps overlay (RMB for more options)");
	ImGui::SameLine();
	ImGui::Checkbox("Alternative Units", &alternative_unit);
  ImGui::Separator();

	ImGuiTabBarFlags tab_bar_flags = ImGuiTabBarFlags_None;
	if (ImGui::BeginTabBar("MyFpsTabBar", tab_bar_flags)) {

		if (ImGui::BeginTabItem("Summary")) {	
			if (ImGui::BeginTable("table1", 5, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_NoHostExtendX)) {
				ImGui::TableSetupColumn("period");
				ImGui::TableSetupColumn("FPS");
				ImGui::TableSetupColumn("min");
				ImGui::TableSetupColumn("avg");
				ImGui::TableSetupColumn("max");
				ImGui::TableHeadersRow();

				draw_summary(story.sum_sec, alternative_unit, 1);
				draw_summary(story.sum_5sec, alternative_unit, 5);
				draw_summary(story.sum_15sec, alternative_unit, 15);

				ImGui::EndTable();
			}
			ImGui::EndTabItem();
		}

		if (ImGui::BeginTabItem("Plotter")) {	
			char overlay[32];
			float values[100] = {0};
			size_t count = 0;

			std::deque<double> &history = story.history;
			if (history.size() > 1) { 
				double old = history[0];
				for (size_t i = 1; i < history.size(); i++) {
						double time = history[i] - old; // frametime
						old = history[i];
						values[count++] = time;
				}
			}

			snprintf(overlay, 32, "avg %.4f, fps %i", story.sum_sec.avg, int(count));
      ImGui::PlotLines("##Lines", values, count, 0, overlay, 0.0f, story.sum_5sec.max, ImVec2(0, 80.0f));
			ImGui::EndTabItem();
		}

		ImGui::EndTabBar();
	}
}