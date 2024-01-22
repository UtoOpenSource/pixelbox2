/*
 * This file is a part of Pixelbox - Infinite 2D sandbox game
 * Evenst for objservers/subscribers patterns
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

#pragma once
#include <functional>
#include <utility>

namespace pb {

	// subject for "Observers"
	template <typename F, typename... Args>
	class Subject {
		protected:
		using function_type = std::function<F(Args...)>;
		std::vector<function_type> handlers;
		public:
		Subject() {};
		Subject(const Subject&) = default;
		Subject(Subject&&) = default;
		Subject& operator=(const Subject&) = default;
		Subject& operator=(Subject&&) = default;
		void subscribe(const function_type& f) { handlers.emplace_back(f); }
		void subscribe(function_type&& f) { handlers.emplace_back(std::move(f)); }
		void notify_all(Args&&... args) {
			for (auto& f : handlers) {
				f(std::forward<Args>(args)...);
			}
		}
	};

};


