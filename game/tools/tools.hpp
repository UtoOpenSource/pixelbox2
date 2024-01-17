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

#pragma once
#include "back_sdl/imgui.h"
#include "base/base.hpp"
#include <functional>
#include <map>
#include <memory>

namespace pb {

	class ToolManager;

	class Tool : public Abstract {
		public:
		bool is_shown = true;
		virtual ~Tool() = 0;
		virtual void operator()(ToolManager& manager) = 0;
		operator bool() const {return is_shown;}
	};

	using ToolConstructor = std::function<std::unique_ptr<Tool>()>;

	class ToolManager {
		private:
		std::map<const HString, std::unique_ptr<Tool>> map;
		std::string save_buffer;
		public:
		ToolManager() = default;
		ToolManager(const ToolManager&) = default;
		ToolManager(ToolManager&&) = default;
		ToolManager& operator=(const ToolManager&) = default;
		ToolManager& operator=(ToolManager&&) = default;
		public:
		bool add_window(const HString& name, std::unique_ptr<Tool>&& src);
		bool add_window(const HString& name, ToolConstructor&& src) {
			return add_window(name, src());
		}
		public:
		void process();
		int  count();
		bool open(const HString& name);
		bool is_opened(const HString& name);
		bool close(const HString& name);
		const std::map<const HString, std::unique_ptr<Tool>>& get_map();
		public:
		void save();
		void load();
		std::string& get_saved();
	};

	namespace tools {
		ToolConstructor CProfiler();
		ToolConstructor CMaster();

		ToolConstructor CMetricsWindow();
		ToolConstructor CLogWindow();
	};

};	// namespace pb