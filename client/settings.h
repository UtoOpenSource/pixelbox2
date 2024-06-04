/*
 * This file is a part of Pixelbox - Infinite 2D sandbox game
 * Settings fro client using SQLITE
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

 #include "engine/raiisqlite.hpp"

namespace pb {

	class SettingsManager {
		protected:
		sqlite::Database db;
		sqlite::Statement get_stmt;
		sqlite::Statement set_stmt;
		public:
		SettingsManager() = default;
		~SettingsManager() = default;
		public:

		template <typename T>
		bool get(const char* id, T& dest) {
			get_stmt.bind(id);
			while (get_stmt.iterate() == SQLITE_ROW) {
				auto res = get_stmt.result();
				if (!res.count()) return false;
				dest = res.get<T>(1);
				return true;
			}
			return false;
		}

		template <typename T>
		void set(const char* id, const T& data) {
			set_stmt.bind(id, data);
			return set_stmt.execute().supress();
		}

	};

};

