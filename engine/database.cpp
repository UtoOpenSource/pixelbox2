/*
 * This file is a part of Pixelbox - Infinite 2D sandbox game
 * Database classes :p
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

#include "database.hpp"

namespace pb {

	bool Statement::iterator() { // return true while has data
		int attemts = 0, res = 0;
		retry:
		res = sqlite3_step(ptr);
		if (res == SQLITE_BUSY) {
			attemts++;
			if (attemts < 100) goto retry;
		}

		bool reset = false;
		if (res == SQLITE_DONE)
			reset = true;
		else if (res == SQLITE_ROW)
			reset = false;
		else {
			sqlite3_reset(ptr);
			throw StatementError(sqlite3_errmsg(sqlite3_db_handle(ptr)));
		}

		if (reset) {
			sqlite3_reset(ptr);
		}
	
		return !reset;	
	}

	bool Statement::column_blob(int index, void* rdst, size_t max) {
		unsigned char* dst = (unsigned char*) rdst;
		const char* src = (const char*)sqlite3_column_blob(ptr, index);
		size_t slen = column_bytes(index);
		if (!src || !max) return false;
		size_t len = slen > max ? max : slen;
		memcpy(dst, src, len);
		return true;
	}

	bool Statement::column_text(int index, std::string& v) {
		const char* src = (const char*)sqlite3_column_text(ptr, index);
		size_t slen = column_bytes(index);
		v.clear();
		if (!src || !slen) return false;
		v.reserve(slen);
		v.append(src, slen);
		return true;
	}

	bool Statement::column_text(int index, char* dst, size_t max) {
		const char* src = (const char*)sqlite3_column_text(ptr, index);
		size_t slen = column_bytes(index);
		if (!src || !max || !(max-1)) return false;
		max--;

		size_t len = slen > max ? max : slen;
		memcpy(dst, src, len);
		dst[len] = '\0'; // end of line
		return true;
	};


	namespace db {

	void make_safe_string(std::string& s) {
		std::replace(s.begin(), s.end(), '"', '_');
		std::replace(s.begin(), s.end(), '\'', '_');
		std::replace(s.begin(), s.end(), '(', '_');
		std::replace(s.begin(), s.end(), ')', '_');
	}

	/** Create sql table if not exists */
	void create_properties_table(Database &db, const char* _name) {
		std::string s("CREATE TABLE IF NOT EXISTS ");
		std::string name = _name;
		make_safe_string(name);

		s += "\""; s += name; s += "\" ";
		s += "(key STRING PRIMARY KEY, value);";
		db.exec(s.c_str());
	}

	/** default database settings */
	void world_settings(Database &db) {
		static const char* init_sql =
		"PRAGMA cache_size = -16000;"
		"PRAGMA journal_mode = MEMORY;"
		"PRAGMA synchronous = 0;"	 // TODO : may be 1 ok?
		"PRAGMA auto_vacuum = 0;"
		"PRAGMA secure_delete = 0;"
		"PRAGMA temp_store = MEMORY;"
		"PRAGMA page_size = 32768;"
		"PRAGMA integrity_check;";
		// TODO: "CREATE TABLE IF NOT EXISTS PROPERTIES (key STRING PRIMARY KEY, "value);"
		// FIXME: "CREATE TABLE IF NOT EXISTS WCHUNKS (id INTEGER PRIMARY KEY, value BLOB);"
		db.exec(init_sql);
	}

	/** default database settings */
	void config_settings(Database &db) {
		static const char* init_sql =
		"PRAGMA cache_size = -16000;"
		"PRAGMA journal_mode = WAL;"
		"PRAGMA synchronous = 0;"	 // TODO : may be 1 ok?
		"PRAGMA auto_vacuum = 0;"
		"PRAGMA secure_delete = 0;"
		"PRAGMA integrity_check;";
		db.exec(init_sql);
	}

	void set_property(Database &db, const char* _table, const char* name, const std::string& value) {
		std::string table = _table;
		make_safe_string(table);

		std::string q = "INSERT OR REPLACE INTO ";
		q += "\""; q += table ; q += "\" VALUES (?1, ?2);";
		auto s = db.query(q.c_str());

		s.bind(1, name);
		s.bind(2, value.c_str(), value.size());
		while (s.iterator()) {}
	}

	void get_property(Database &db, const char* _table, const char* name, std::string& value) {
		std::string table = _table;
		make_safe_string(table);
		value.clear();

		std::string q = "SELECT value FROM ";
		q += "\""; q += table ; q += "\" WHERE key = ?1;";
		auto s = db.query(q.c_str());

		s.bind(1, name);
		while (s.iterator()) {
			value = s.column<std::string>(0);
		}
	}

	};

};
