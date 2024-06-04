/*
 * Copyright (C) UtoECat 2024
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the “Software”), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#include "raiisqlite.hpp"
#include <cstring>
#include <exception>
#include <new>
#include <sstream>
#include <stdexcept>
#include "sqlite3.h"

// implementation
namespace sqlite {

DatabaseError Statement::compile(sqlite3 *db, Text& src, int flags) noexcept {
	release(); 

	const char *left = src.data();
	int e = SQLITE_EMPTY;	// empty statement code

	while (left < src.end()) { // while we don't consume entire statement
		e = sqlite3_prepare_v3(db, src.data(), src.size(), flags, &stmt, &left);
		if (e != SQLITE_OK) {	 // if error - exit now
			break;
		}

		// skip whitespaces (in case of SQLITE_OK and no statement)
		while (left < src.end() && isspace(left[0])) left++;

		// recalculate current pos and size
		src = Text(left, src.end() - left);

		// if no error (SQLITE_OK and statement exists) => break after skip
		if (stmt) break;

		// no data available here (SQLITE_OK but no statement)
		// if we still have stuff to cionsume - this will be overriden
		e = SQLITE_EMPTY;
	};

	return e;
}

static const std::string OOM_MSG = "OOM";

std::string Statement::get_compile_error(sqlite3* db, const Text start, const Text curr) noexcept {
	const char* limit = curr.data() ? curr.data() : start.data();

	try {
		if (!start.data() || !limit) 
			return std::string("get_comile_error misuse : no source code or valid leftover"); // ERROR
		if (limit < start.begin() || limit > start.end())
			return std::string("get_comile_error misuse : size overflow or curr is not a subset of start!"); // ERROR
	} catch (std::bad_alloc& e) {
		return OOM_MSG; // hopefully will not make another allocation
	}

	int line = 1; // current line
	int pos  = 1; // position in the line

	// calculate position (curr) in source code (start)
	const char* v = start.data();
	const char* prev_begin = v; // first char of current line to start code preview from
	for (; v < limit; v++) {
		pos++;
		if (*v == '\n') {
			line++;
			prev_begin = v + 1;
			pos = 1;
		}
	}

  // cool error message :3
	try {
		std::stringstream out;

		out << sqlite3_errstr(sqlite3_errcode(db)) << " : " << sqlite3_errmsg(db) << std::endl;

		size_t spacing = out.tellp();

		out << "" << line << ":" << pos << ": "; 

		spacing = (size_t)out.tellp() - spacing;

		// problematic line source preview
		const char* preview = prev_begin;
		int prev_count = 0;
		bool was_space = 0;

		while (preview < start.end()) {
			if (*preview == '\n') {
				prev_count++;
				if (prev_count >= 2) break; // max  2 lines of code
			}

			bool is_space = isspace(preview[0]);
			if (preview[0] != '\n' && is_space && !was_space) {
				out << ' ';
				was_space = 1;
			} else if (!is_space) {
				out << preview[0];
				was_space = 0;
			} else {
				out << "\n";
				for (size_t i = 0; i < spacing - 2; i++) out << ' ';
				out << ':';
			}

			preview++;
		}

		return out.str();
	} catch (std::exception& e) { // Out of memory?
		return OOM_MSG;
	};
}

DatabaseError Statement::iterate() noexcept {
	// original sqlite_step() returns SQLITE_OK... and it's counterintuitive
	if (!stmt) return SQLITE_EMPTY;	
	int rc = sqlite3_step(stmt);

	if (rc == SQLITE_ROW) { // else OK or DONE, other are errors (including SQLITE_BUSY)
		return rc;
	}

	int rc2 = sqlite3_reset(stmt);
	if (rc == SQLITE_OK && rc2 != SQLITE_OK) rc = rc2;	// more error info
	return rc; // error or done
}

DatabaseError Backup::start(sqlite3 *src, sqlite3 *dest, const char* schema_name) {
		if (!src || !dest) throw std::runtime_error("database is nullptr!");
		if (!schema_name) schema_name = "main";
		ctx = sqlite3_backup_init(dest, schema_name, src, schema_name);
		if (!ctx) {
			return DatabaseError(sqlite3_errcode(dest));
		}
		return DatabaseError(SQLITE_OK); // OK
}

DatabaseError Backup::iterate(int n_pages) noexcept {
	if (!ctx) return SQLITE_EMPTY;	// error?
	int err;

	err = sqlite3_backup_step(ctx, n_pages);
	if (err == SQLITE_OK || err == SQLITE_BUSY || err == SQLITE_LOCKED) {
		return err;	 // should continue...
	}

	// not recoverable OR done
	// THERE WAS CALL TO destroy() AND THAT WAS A MISTAKE! ITERATION FUNCTION MUST NOT CLOSE HANDLE!
	return err;
}

void Database::close() noexcept {
	if (db) sqlite3_close_v2(db);
	db = nullptr;
}

DatabaseError Database::raw_open(const char *url, int flags) noexcept{
	close();
	int e = sqlite3_open_v2(url, &db, flags, nullptr);
	return e;
}

std::string Statement::expanded_sql() {
	std::string v;
	const char* s = sqlite3_expanded_sql(stmt);
	try {
		if (s) {
			v = s;
		}
	} catch (...) {};
	sqlite3_free((void*)s);
	return v;
}

static void db_open_check(DatabaseError e) {
	std::string errmsg = "Can't open database : ";
	errmsg += sqlite3_errstr(e.get());
	errmsg += "!";

	if (e != SQLITE_OK) throw std::runtime_error(errmsg);
}

Database connect(const char* url, bool readonly, bool ignore_not_exists) {
	Database db;
	int flags = 0;
	flags |= readonly ? SQLITE_OPEN_READONLY : SQLITE_OPEN_READWRITE;
	if (!readonly && ignore_not_exists)
		flags |= SQLITE_OPEN_CREATE;
	
	db_open_check(db.raw_open(url, flags));
	return db;
}

Database create_memory(const char* url) {
	Database db;
	int flags = SQLITE_OPEN_READWRITE | SQLITE_OPEN_MEMORY;
	db_open_check(db.raw_open(url, flags));
	return db;
}

Database connect_uri(const char* url) {
	Database db;
	// last one are required!
	db_open_check(db.raw_open(url, SQLITE_OPEN_URI | SQLITE_OPEN_CREATE | SQLITE_OPEN_READWRITE));
	return db;
}

Database& Database::operator=(Database && src) {
	if (this == &src) return *this; 
	if (db) close(); // important!
	db = src.db;
	src.db = nullptr;
	return *this;
}

};	// namespace sqlite