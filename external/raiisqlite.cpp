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
#include <sstream>
#include "sqlite3.h"

// implementation
namespace sqlite {

DatabaseError Statement::compile(sqlite3 *db, Text src, const char **leftover, int flags) noexcept {
	release();
	const char *left = src.src;
	int e = SQLITE_EMPTY;	// empty statement code

	while (left < src.src + src.size) {
		e = sqlite3_prepare_v3(db, src.src, src.size, flags, &stmt, &left);

		if (e != SQLITE_OK) {	 // if error - exit now
			break;
		}

		// else skip whitespaces (SQLITE_OK and no statement)
		// for OK case it is IMPORTANT to return proper error code!
		// (we will return SQLITE_EMPTY if not skip spaces, and that's
		// invalid behaviour)
		while (left < src.src + src.size && isspace(left[0])) left++;

		// recalculate current pos and size
		src.size = (src.src + src.size) - left;
		src.src = left;

		// if no error (SQLITE_OK) => break after skip
		if (stmt) {
			break;
		}

		// no data available here
		e = SQLITE_EMPTY;
	}

	// write position in code
	if (leftover) *leftover = left;
	return e;
}

std::string Statement::get_compile_error(sqlite3* db, Text src, const char * const* leftover) noexcept {
	const char* limit = leftover ? *leftover : src.src;
	if (!src.src || !leftover) 
		return std::string("get_comile_error misuse : no source code or valid leftover"); // ERROR
	if (limit < src.src || limit > src.src + src.size)
		return std::string("get_comile_error misuse : size overflow"); // ERROR

	int line = 1; // current line
	int pos  = 1; // position in the line

	// calculate position in source code
	const char* v = src.src;
	const char* prev_begin = src.src;
	for (; v < limit; v++) {
		pos++;
		if (*v == '\n') {
			line++;
			prev_begin = v + 1;
			pos = 1;
		}
	}

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
		while (preview < src.src + src.size) {
			if (*preview == '\n') {
				prev_count++;
				if (prev_count >= 2) break;
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
		return std::string("nomem");
	};
}

DatabaseError Statement::iterate() noexcept {
	// original sqlite_step() returns SQLITE_OK... and it's counterintuitive
	if (!stmt) return SQLITE_EMPTY;	
	int rc = sqlite3_step(stmt);

	if (rc == SQLITE_ROW) {
		return rc;
	}

	// if (rc != SQLITE_ROW) => reset NOW
	int rc2 = sqlite3_reset(stmt);
	if (rc == SQLITE_OK && rc2 != SQLITE_OK) rc = rc2;	// more error info
	return rc; // error or done
}

Text::Text(const char *s, bool nocopy) : src(s), size(strlen(s)), is_static(nocopy) {}

DatabaseError Statement::execute(ColumnCallback cb) noexcept {
	if (!stmt) return SQLITE_EMPTY;
	DatabaseError rc;

	if (cb) {
		try {
			while ((rc = iterate()) == SQLITE_ROW) {
				cb(result());	 // execute callback
			}
		} catch (...) {	 // callback aborted
			reset();
			rc = SQLITE_ABORT;	// don't retrow!
		} 
	} else {
		while ((rc = iterate()) == SQLITE_ROW) {}
	}

	return rc; // should be moved... and not trigger unchecked error...
}

DatabaseError Backup::execute_until_busy(int n_pages) {
	DatabaseError err;
	do {
		err = iterate(n_pages);
		sleep();
	} while (err == SQLITE_OK);

	if (err != SQLITE_DONE && err != SQLITE_BUSY && err != SQLITE_LOCKED) {
		throw DatabaseException(sqlite3_errstr(err.get()));
	}
	return err;
}

void Backup::execute_sync(int n_pages) {
	DatabaseError err;

	do {
		err = execute_until_busy(n_pages);
	} while (err == SQLITE_OK || err == SQLITE_BUSY);

	if (err != SQLITE_DONE) throw DatabaseException(sqlite3_errstr(err.get()));
}

void Backup::execute_now() {
	struct anon {
		Backup* me;
		int old;
		anon(Backup* a, int b) : me(a), old(b) {} 
		~anon() {
			me->sleep_amount = old;
		}
	} _(this, sleep_amount);

	sleep_amount = 0; // no delays 
	execute_sync(100); // let's go
};

DatabaseError Backup::iterate(int n_pages) noexcept {
	if (!ctx) return SQLITE_EMPTY;	// error?
	int err;

	err = sqlite3_backup_step(ctx, n_pages);
	if (err == SQLITE_OK || err == SQLITE_BUSY || err == SQLITE_LOCKED) {
		return err;	 // should continue...
	}

	// not recoverable OR done
	destroy();
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

namespace impl {

DatabaseError StaticQueryStorage::compile(Text sql_code) noexcept {
	statements.clear();
	index = 0;

	Statement stmt;
	const char *leftover = nullptr;
	DatabaseError err;

	errmsg.clear();
	while ((err = stmt.compile(database, sql_code, &leftover, SQLITE_PREPARE_PERSISTENT)) == SQLITE_OK) {
		// psuh statement
		statements.push_back(std::move(stmt));

		// move current text pointer
		sql_code.size = sql_code.src + sql_code.size - leftover;
		sql_code.src = leftover;
	};

	if (err != SQLITE_EMPTY) { // means error
		errmsg = stmt.get_compile_error(database, sql_code, &leftover);
		statements.clear();
		rewind();
		return err;
	}

	// good ending :З
	return SQLITE_OK;
}

DatabaseError InterpretQueryStorage::rewind() noexcept {
		cpos = sql.begin();
		stmt.release();

		DatabaseError err = stmt.compile(database, curr(), &cpos);

		// save error message
		if (err != SQLITE_OK) {
			make_errmsg();
			cpos = sql.begin(); // rewind code position
		};

		return err; // PASS SQLITE_EMPTY! Since empty SQL is an error from the start for us!
	}

	DatabaseError InterpretQueryStorage::next() noexcept {
		if (!stmt) return SQLITE_MISUSE;

		DatabaseError err = stmt.compile(database, curr(), &cpos);

		// save error message
		if (err != SQLITE_OK && err != SQLITE_EMPTY) {
			make_errmsg();
			return err;
		};

		return err == SQLITE_EMPTY ? SQLITE_DONE : SQLITE_OK; // turn EMPTY to DONE!
	}

};	// namespace impl

static void db_open_check(DatabaseError e) {
	std::string errmsg = "Can't open database : ";
	errmsg += sqlite3_errstr(e.get());
	errmsg += "!";

	if (e != SQLITE_OK) throw DatabaseException(errmsg, e.get());
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