/*
 * This file is a part of Pixelbox - Infinite 2D sandbox game
 * Ultimate settings storage
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
 #include "printf.h"

namespace pb {

	class SettingsManager {
		protected:
		sqlite::Database db;
		sqlite::Statement get_stmt;
		sqlite::Statement set_stmt;
		void init_stmts(const char* tab) {
			char tmp[512];
			int n = snprintf_(tmp, 512, "CREATE TABLE IF NOT EXISTS %s (key STRING PRIMARY KEY NOT NULL, value)", tab);
			if (n > 0) {
				std::string_view sql(tmp, n);
				sqlite::Statement stmt;
				stmt.compile(db, sql).raise();
				stmt.execute().raise();
			};

			n = snprintf_(tmp, 512, "SELECT value FROM %s WHERE key = ?1", tab);
			if (n > 0) {
				std::string_view sql(tmp, n);
				get_stmt.compile(db, sql).raise();
			}
			n = snprintf_(tmp, 512, "INSERT OR REPLACE INTO %s(key, value) VALUES (?1, ?2)", tab);
			if (n > 0) {
				std::string_view sql(tmp, n);
				set_stmt.compile(db, sql).raise();
			}
		}
		public:
		SettingsManager() = default;
		public:

		template <typename T>
		bool get(const char* id, T& dest) {
			get_stmt.bind(id);
			bool stat = false;
			while (get_stmt.iterate() == SQLITE_ROW) {
				auto res = get_stmt.result();
				if (!res.count()) return false;
				dest = res.get<T>(0);
				LOG_DEBUG("get %s success (%s)", id, res.get<std::string>(0).c_str());
				stat = true;
			}
			return stat;
		}

		template <typename T>
		void set(const char* id, const T& data) {
			set_stmt.bind(id, data);
			return set_stmt.execute().raise();
		}

		/// open database that will be managed By settings manager
		void open(const char* dbname) {
			close();
			db = sqlite::Database(dbname);
			init_stmts("pb_settings");
		}

		// USE database managed externally. This handler MUST NOT BE FREED UNTIL DESTRUCTION OF THIS MANAGER!
		void use_handler(sqlite3* h, const char* tabname="pb_settings") {
			db.from_raw(h, false); // don't manage it! caller is responcible for freeing database handle!
			init_stmts(tabname);
		}

		void close() {
			get_stmt.release();
			set_stmt.release();
			db.close();
		}

		~SettingsManager() {close();}
	};

};

