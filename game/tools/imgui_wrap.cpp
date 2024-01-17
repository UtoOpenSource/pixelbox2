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

namespace pb {

namespace tools {

class ImGuiWrap : public Tool {
	public:
	~ImGuiWrap() {}
};

class MetricsWindow : public ImGuiWrap {
	public:
	void operator()(ToolManager&) {
		ImGui::ShowMetricsWindow(&is_shown);
	}
};

ToolConstructor CMetricsWindow() {
	return []() { return std::make_unique<MetricsWindow>();};
}

class LogWindow : public ImGuiWrap {
	public:
	void operator()(ToolManager&) {
		ImGui::ShowDebugLogWindow(&is_shown);
	}
};

ToolConstructor CLogWindow() {
	return []() { return std::make_unique<LogWindow>();};
}


};	// namespace tools

};	// namespace pb