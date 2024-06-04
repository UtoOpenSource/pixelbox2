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

#pragma once
#include <cstdint>
#include <stdexcept>
#include <string_view>
#include "bindata.hpp"
#include "sqlite3.h"

/// predefinition
namespace sqlite {

/** 
* @brief wrapper around sqlite3_stmt. No copyable, movable only.
* statement will be destroyed when object will destroyed (if any)
* you can recreate statement with new source (and new internal object)
* as well as release() statement early
*
* @warning you MUST NOT release statement if it will be used after destruction
* of this wrapper! (that's probably bad design of your program).
* there is no way to get ownership of created statement! ...to prevent bad design
* Hovewer, you still can std::move() this object somewhere... If it's really important.
*/
class Statement;

/**
 * @brief represents result of the Query/Statement step
 * 
 * @warning when query is mutated by it's methods (release()/iterate()/etc...)
 * or destructed, you cannot use this object anymore. 
 * It is recommended to never store this object for long time anywhere.
 *
 * This object will likely to become invalid after callback function returns
 * or throws! (because the Statement may be released!)
 */
class QueryResult;

/**
* @brief RAII Database wrapper. 
* 
* @warning You must destroy all objects that uses it before destructing/closing database.
* This includes Statement, Query, Program and Backup! 
*/
class Database;

/** Optional error, returned by most of the functions. */
//class DatabaseError;

/** 
 * @brief Step-by-step database backup routine
 * @warning both databases, passed in constructor, must exists and not change
 * while Backup routine is not finished.
 *
 * Not changed means not reopened and not closed. You may still use
 * source database in breaks between iterate() calls... (but not dest)
 */
class Backup;

using Byte = pb::Byte;

};	// namespace sqlite

/// definition
namespace sqlite {

/** If enabled (default) any unhandled(unchecked) Database 
 * Error will print to stderr about being unhandled!
 */
static constexpr bool REPORT_UNHANDLED_ERRORS = true;

namespace impl {

template <bool report>
class DbError;

template<> // no error report, mosrt checks are optimized out
class DbError<false> {
	friend class Testing;
	protected:
	int errcode = 0;
	public:
	bool is_error() const {
		int is_ok = 0;
		is_ok |= (errcode == SQLITE_OK);
		is_ok |= (errcode == SQLITE_DONE);
		is_ok |= (errcode == SQLITE_ROW);
		return !is_ok;
	}
	public:
	inline DbError() {}
	inline DbError(int sqlerr) {set_error(sqlerr);}
	// movement optimized
	inline DbError(DbError&& src) {*this = std::move(src);}
	DbError& operator=(DbError&& src) {errcode = src.errcode;return *this;}
	public:
	/** set error */
	inline void set_error(int sqlerr) {errcode = sqlerr;}
	// convinient operators
	inline bool operator==(int sqlerr) {return errcode == sqlerr;}
	inline bool operator!=(int sqlerr) {return !(*this == sqlerr);}

	[[nodiscard]] inline bool check() {return !is_error();} // error is checked
	[[nodiscard]] inline int get() {return errcode;} // error is kinda checked?
	inline void supress() {} // threat as we don't care about this error at all
	inline void raise() {if (is_error()) throw std::runtime_error(sqlite3_errstr(errcode));} // raise exception on error
	inline DbError copy() const {return DbError(errcode);}

	inline ~DbError() = default;
};

template<> // with error report andf check
class DbError<true> : public DbError<false> {
	friend class Testing;
	protected:
	bool is_checked=false;
	public:
	using DbError<false>::DbError;
	public:

	// construction reqires recheck
	inline DbError(DbError&& src) {
		*this = std::move(src);
		is_checked = 0;
	}

	/** copy error and errstatus, and set is_checked on source */
	DbError& operator=(DbError&& src) {
		errcode = src.errcode;
		is_checked = src.is_checked;
		src.is_checked = 1;
		return *this;
	}

	[[nodiscard]] inline bool check() {is_checked = 1; return !is_error();} // error is checked
	[[nodiscard]] inline int get() {is_checked = 1; return errcode;} // error is kinda checked?
	inline void supress() {is_checked = 1;} // threat as we don't care about this error at all
	inline void raise() { // raise exception on error
		if (is_error()) {
			is_checked = true;
			throw std::runtime_error(sqlite3_errstr(errcode));
		}
	}
	inline DbError copy() const {return DbError(errcode);}

	// to prevent misuse. use get() method for that!
	operator int() = delete;

	// convinient operators
	[[nodiscard]] inline bool operator==(int sqlerr) {is_checked = 1;return errcode == sqlerr;}
	[[nodiscard]] inline bool operator!=(int sqlerr) {return !(*this == sqlerr);}

	inline ~DbError() {
			if (is_error() && !is_checked) fprintf(stderr, "unhandled Database Error : %s, %i\n", sqlite3_errstr(errcode), errcode);
	}

};

};

using DatabaseError = impl::DbError<REPORT_UNHANDLED_ERRORS>;
using Text = std::string_view;
using Blob = pb::BytesView;

/** implementation specific stuff. Do not use directly! */
namespace impl {

/** raw bind value to statement */
inline bool sqlite_unbind(sqlite3_stmt *stmt, int idx) { return sqlite3_bind_null(stmt, idx); }

/// use macros to keep my sanity healthy
#define _TMP_MAGIC_SHIT(TYPE, SIGNA, ...) \
	inline bool sqlite_bind(sqlite3_stmt *stmt, int idx, [[maybe_unused]] TYPE v) { return sqlite3_bind_##SIGNA(stmt, idx, ##__VA_ARGS__); }

_TMP_MAGIC_SHIT(std::nullptr_t, null);
_TMP_MAGIC_SHIT(int, int, v);
_TMP_MAGIC_SHIT(unsigned int, int, v);
_TMP_MAGIC_SHIT(int64_t, int64, v);
_TMP_MAGIC_SHIT(uint64_t, int64, v);
_TMP_MAGIC_SHIT(double, double, v);
_TMP_MAGIC_SHIT(float, double, v);

// ok, we will do some manual shit here
inline bool sqlite_bind(sqlite3_stmt *stmt, int idx, Text v) {return sqlite3_bind_text(stmt, idx, v.data(), v.size(), SQLITE_STATIC);};
inline bool sqlite_bind(sqlite3_stmt *stmt, int idx, Blob v) {return sqlite3_bind_blob(stmt, idx, v.begin(), v.length(), SQLITE_STATIC);};

inline bool sqlite_bind_copy(sqlite3_stmt *stmt, int idx, Text v) {return sqlite3_bind_text(stmt, idx, v.data(), v.size(), SQLITE_TRANSIENT);};
inline bool sqlite_bind_copy(sqlite3_stmt *stmt, int idx, Blob v) {return sqlite3_bind_blob(stmt, idx, v.begin(), v.length(), SQLITE_TRANSIENT);};

#undef _TMP_MAGIC_SHIT

/** raw column */
template <typename T>
inline T sqlite_column(sqlite3_stmt *, int) {
	static_assert("not implemented!");
}

template <>
inline std::nullptr_t sqlite_column<std::nullptr_t>(sqlite3_stmt *, int) {return nullptr;}

template <>
inline void sqlite_column<void>(sqlite3_stmt *, int) {}

#define _TMP_COLUMN_SHIT(TYPE, SIGNA)                             \
	template <>                                                     \
	inline TYPE sqlite_column<TYPE>(sqlite3_stmt * stmt, int idx) { \
		return sqlite3_column_##SIGNA(stmt, idx);                     \
	}

_TMP_COLUMN_SHIT(int, int);
_TMP_COLUMN_SHIT(unsigned int, int);
_TMP_COLUMN_SHIT(int64_t, int64);
_TMP_COLUMN_SHIT(uint64_t, int64);
_TMP_COLUMN_SHIT(double, double);
_TMP_COLUMN_SHIT(float, double);

#undef _TMP_COLUMN_SHIT

template <>
inline Text sqlite_column<Text>(sqlite3_stmt *stmt, int idx) {
	return Text((char *)sqlite3_column_text(stmt, idx), (size_t)sqlite3_column_bytes(stmt, idx));
}

template <>
inline Blob sqlite_column<Blob>(sqlite3_stmt *stmt, int idx) {
	return Blob(sqlite3_column_blob(stmt, idx), (size_t)sqlite3_column_bytes(stmt, idx));
}

/** copy data */
template <>
inline std::string sqlite_column<std::string>(sqlite3_stmt *stmt, int idx) {
	return std::string(sqlite_column<Text>(stmt, idx));
}

};	// namespace impl

class QueryResult {
 friend class Testing;
 protected:
	friend class Statement;
	sqlite3_stmt *stmt;
	QueryResult(sqlite3_stmt *v) : stmt(v) {
		if (v == nullptr) throw std::runtime_error("nullptr statement!");
	}
	
 public:
	QueryResult(const QueryResult &) = default;
	QueryResult(QueryResult &&) = default;
	QueryResult &operator=(const QueryResult &) = delete; // error-prone 
	QueryResult &operator=(QueryResult &&) = default;

	/** return count of avaliable data. Max valid index is size()-1 */
	size_t count() const { return sqlite3_data_count(stmt); }

	/** return length of the string or blob at this index */
	size_t length(int index) { return sqlite3_column_bytes(stmt, index); }

	/** get column name */
	const char* name(int index) { return sqlite3_column_name(stmt, index); }

	/** recieve data at specified index as specified type. Sqlite may do some convertions
	 * under the hood! */
	template <typename T>
	T get(int index) {
		return impl::sqlite_column<T>(stmt, index);
	}

	/** return SQLITE_XXXX type */
	int type(int index) { return sqlite3_column_type(stmt, index); }
};

class Statement {
 private:
  friend class Testing;
	sqlite3_stmt *stmt = nullptr;
 public:

	Statement() = default;
	Statement(const Statement &) = delete;
	Statement &operator=(const Statement &) = delete;
	inline Statement(Statement &&src) { *this = std::move(src); }
	inline Statement &operator=(Statement &&src) {
		if (this == &src) return *this;
		stmt = src.stmt;
		src.stmt = nullptr;
		return *this;
	};

	/** may be in invalid(released) state! check if statement actualy exists! */
	operator bool() const noexcept { return !!stmt; }

	/** releases statement early */
	inline void release() noexcept {
		if (stmt) sqlite3_finalize(stmt);
		stmt = nullptr;
	}

	/** creates statement from specified SQL code. Will set src string view to the rest of not used sql code.
	 * also accepts SQLITE_PREPARE_* flags
	 * it will ignore empty spaces in SQL code, however, will return
	 * @return SQLITE_OK or error code (SQLITE_EMPTY for no code).
	 * @warning this operation ALWAYS destroys previous statement (and invalidates
	 * old statement pointer!)
	 *
	 * you may get compilation error using get_compile_error() method, HOWEVER, it has some limitations.
	 * see @ref get_compile_error() docs for more info.
	 */
	[[nodiscard]] DatabaseError compile(sqlite3 *db, Text& src, int flags = 0) noexcept;

	/** Executes statement.
	 * returns SQLITE_DONE => execution is complete successfully, SQLITE_ROW => we have result data to process, any other value => error.
	 */
	[[nodiscard]] DatabaseError iterate() noexcept;

	/** get results from statetment.
	 * You can only call this when iterate() returns SQLITE_ROW!
	 * @warning see warnings in QueryResult class documentartion!
	 * @warning DO NOT call on empty statement!
	 */
	inline QueryResult result() const {return QueryResult(stmt);}

	/** resets statement executon position.
	 * you may call iterate() again after that :)
	 * but you shouldn't call this AFTER iterate()!
	 * internally iterate always resets statement when it
	 * finishes or returns an ERROR! 
	 */
	inline void reset() noexcept {if (stmt) sqlite3_reset(stmt);}

	/** just do it
	 * pass callback to retrieve all row results
	 * return non-zero to abort
	 */
	[[nodiscard]] DatabaseError execute(auto& cb = nullptr) noexcept {
		if (!stmt) return SQLITE_MISUSE;
		DatabaseError rc;
			while ((rc = iterate()) == SQLITE_ROW) {
				 // execute callback
				if (cb(result()) != 0) {
					reset();
					rc = SQLITE_ABORT;	// callback aborted
					break;
				}
			}
		return rc; // busy is not handled, because we don't have database BLOCKED in pixelbox, it's opened exclusively
	}

	/**
	 * fast path, without callback and any exceptions
	 */
	[[nodiscard]] DatabaseError execute() noexcept {
		if (!stmt) return SQLITE_MISUSE;
		DatabaseError rc;
		while ((rc = iterate()) == SQLITE_ROW) {}
		return rc;
	}

	/** bind arguments passed to function... some black magic :D */
	template <typename... Args>
	void bind(Args &&...args) noexcept {
		if (!stmt) return;
		const int lim = sqlite3_bind_parameter_count(stmt);

		// return early
		if (lim < 1) return;

		/** bind values */
		int i = 1;	// start from 1

		(
				[&] {
					if (i <= lim) impl::sqlite_bind(stmt, i, args);
					++i;
				}(),
				...);
	}

	/** unbind all parameters */
	void unbind() {sqlite3_clear_bindings(stmt);}

	/** DO NOT CONFUSE WITH ERROR SQLITE_BUSY! THis one means that statement was started to be executed, but was not finished!
	 * you should continue doing iterate() or reset() it!
	 */
	bool is_busy() {return sqlite3_stmt_busy(stmt);}

	/** for debugging purposes. */
	std::string expanded_sql();

	/** pretty error ouput
	  * @warning SQLITE keeps compilation error message instde database handle.
		* this means that you will not get valid error message if you will trigger
		* any other error after compile error and between call to this function! (incluing parallel threads!)
		*
		* db is a database, passed in compile() routine.
		* start must be antire original SQL code
		* curr must be SQL code, modifid by comile() list time (it contains corrent offset)
		*
		* returns empty string on error. Or empty error message.
		*/
	std::string get_compile_error(sqlite3* db, const Text start, const Text curr) noexcept;

	operator sqlite3_stmt *() noexcept { return stmt; } // rawdog pointer

	~Statement() { release(); }
};

class Backup {
 friend class Testing;
 protected:
	sqlite3_backup *ctx = nullptr;
 public:
	Backup(const Backup &) = delete;
	Backup(Backup &&) = default;
	Backup &operator=(const Backup &) = delete;
	Backup &operator=(Backup &&) = default;
	Backup() = default;

	// fast init
	template <typename... Args>
	inline Backup(sqlite3 *src, sqlite3 *dest, Args&&... args) {
		if (!src || !dest) throw std::runtime_error("database is nullptr!");
		if (!start(src, dest, std::forward<Args>(args)...).check()) {
			throw std::runtime_error(sqlite3_errmsg(dest)); // it has more complete error message
		}
	}

	operator bool() {
		return !!ctx;
	}

	/** initializes backup operation
	 * returns errors, not throws. 
	 * you can get error message from dest database!
	 *
	 * set delay argument to zero to never sleep in
	 * execute() routines! 
	 * this may be important in cases you know that 
	 * databases will never be locked/busy, or busy for long
	 *
	 * i don't know why i did all this shit.. but anyway
	 */
	[[nodiscard]] DatabaseError start(sqlite3 *src, sqlite3 *dest, const char* schema_name = "main");

	int remaining() const noexcept { return ctx ? sqlite3_backup_remaining(ctx) : 0; }
	int length() const noexcept { return ctx? sqlite3_backup_pagecount(ctx) : 0; }
	int position() const noexcept { return length() - remaining(); }

	// wait to allow db to work a bit in background.
	// does not called by any functions. It is for you to optionally call it.
	static void sleep(int sleep_amount) noexcept {
		sqlite3_sleep(sleep_amount);
	}

	/** may return error 
	 * you can discard it. kinda
	 */
	DatabaseError destroy() {
		if (!ctx) return SQLITE_MISUSE;

		/* Release resources allocated by backup_init(). */
		int err = sqlite3_backup_finish(ctx);
		ctx = nullptr;
		return err;
	}

	/** iterator. returns SQLITE_*** codes.
	 * you should explicitly handle SQLITE_BUSY and SQLITE_LOCKED errors
	 * Errors like SQLITE_NOMEM, SQLITE_READONLY, SQLITE_IO_* ARE FATAL! (aka there is no point to try call iterate() again)
	 * SQLITE_DONE notifies about SUCCESS and finish!
	 * SQLITE_OK notifies about succesfull step (you should call this func again)
	 * does not throw exceptions
	 * if n_pages == -1, it will copy an entire database in one call
	 *
	 * you may want to call destroy() in case of error or succes. Destructor calls it too.
	 */
	[[nodiscard]] DatabaseError iterate(int n_pages = 10) noexcept;

	// DEPRECATED AND REMOVED
	//DatabaseError execute_until_busy(int n_pages = 10);
	//void execute_sync(int n_pages = 10);
	//void execute_now();

	/// rolls back everything if backup was not finished!
	~Backup() { destroy().supress(); }
};

/** opens database at specified path OR creatres new one.
 * database is ALWAYS opened in readwrite mode!
 * throws on errors.
 */
Database connect_or_create(const char* url);

/** opens database using URI.
 * it allows to pass all arguments directly using one string, extra args to select VFS and so on.
 * see https://www.sqlite.org/c3ref/open.html URI filenames section fore more info, warnings and examples
 * @warning you REALLY should visit link above, because certain URI settings can lead to database corruption
 * @example "file:///home/fred/data.db"
 * @example "file:data.db?mode=ro&cache=private"
 * @example "file:/home/fred/data.db?vfs=unix-dotfile"
 */
Database connect_uri(const char* uri);

class Database {
	friend class Testing;
	sqlite3 *db = nullptr;
 public:
	Database() = default;
	Database(const Database &) = delete;
	Database &operator=(const Database &) = delete;

	// v.1.1 allow move construction
	Database(Database && src) {
		db = src.db; src.db = nullptr;
	}

	/** there is no way to copy database handler, and should not be any...
	 * well no, it is possible if iopen same database file multiple times with shared cache and WAL but it's tricky. AND prefomance intensive.
	 * better to use separate thread and SQL Pool for that.
	 */
	Database &operator=(Database && src);

	Database(const char *url) : Database(connect_or_create(url)) {}
	~Database() { close(); }

	public:
	operator bool() const { return !!db; }
	/**
	 * closes database handler
	 * @warning you must not close/destroy database while it's handler is used somewhere else
	 * (for example in Statements/Queries/Programs created from it, and objects created from them)
	 */
	void close() noexcept;

	/** raw database open function
	 * does not throw exceptions. Used by factories-functions!
	 * and CAN be used directly! 
	 */
	[[nodiscard]] DatabaseError raw_open(const char *path, int flags) noexcept;

	// theese functions removed to be replaced by explicit factories and prevent accidental mistakes.
	// raw_open will still be availab;le, because it is a "PRO" one.
	/// same as sqlite::connect_or_create()!
	//void open(const char* path) {*this = sqlite::connect_or_create(path);}
	/// same as sqlite::connect_uri()!
	//void open_uri(const char* uri) {*this = sqlite::connect_uri(uri);}

	/** flushes all cache into disk to write results. */
	void flush() {
		if (db) sqlite3_db_cacheflush(db);
	}

	/** attempts to free as much memory (RAM) as possible. Use in case of OOM
	 * You should'nt do it in other cases, because it will flush RAM cache and will force
	 * database to read all shit from the disk again. And this is slow.
	 */
	void shrink_to_fit() {
		if (db) sqlite3_db_release_memory(db);
	}

	/** check is database readonly. result on closed/not opened database is undefined. */
	bool is_readonly() { return db ? sqlite3_db_readonly(db, "main") : false; }

	/** fire and forget. sqlite3_exec()-like, but with bind() available (and without row callback). throws on error! */
	template <typename... Args>
	inline void exec(Text sql, Args&& ...args) {
		if (!db) throw std::runtime_error("database is not opened!");
		Text curr = sql;
		Statement stmt;
		DatabaseError err;

		while ((err = stmt.compile(db, curr)) == SQLITE_OK) {
			stmt.bind(std::forward<Args>(args)...);
			err = stmt.execute();
			if (err != SQLITE_DONE) break;
		}
		
		if (err != SQLITE_EMPTY) { // oh no
			throw std::runtime_error(stmt.get_compile_error(db, sql, curr));
		}
	};

 public:
	operator sqlite3 *() const { return db; }
};

/** opens database at specified path with default settings 
 * will NOT attempt to create database, if it does not exists by default!
 * throws on errors.
 * readonly parameter is self-explainatory. 
 * ignore_not_exists - database will be created, if not exists and mode is NOT readonly
 */
Database connect(const char* path, bool readonly = false, bool ignore_not_exists = false);

/** opens database at specified path OR creatres new one.
 * database is ALWAYS opened in readwrite mode!
 * throws on errors.
 *
 * this is recommended call. Especially when you have all schema being reexecuted 
 * exery time app starts (which is a good thing)
 */
inline Database connect_or_create(const char* path) {
	return connect(path, false, true);
}

/** opens an empty in-memory database. No data will be readed/writed from/to disk!
 */
Database create_memory(const char* path = "");

};
