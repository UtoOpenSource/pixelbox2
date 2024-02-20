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
#include <exception>
#include <functional>
#include <stdexcept>
#include <string>
#include <string_view>
#include "sqlite3.h"

/// predefinition
namespace sqlite {

// uniform friend for unittesting!
class Testing;

/** Sqlite value wrappers to make advanced bind() possible
 * @warning string and blob values threated as non-static by default
 * and that's why they will be copied internally by Sqlite.
 * for specific cases, you may want to override this,
 * by passing true in text/blob constructor, but be careful!
 * Texts and Blobs with is_static==true MUST NOT be changed or freed
 * during a whole ass lifetime of the Query AND associated Statement
 *
 * @warning Text and Blob returned from column() are ALWAYS static!
 * this means that you MUST copy them or move value from them to use
 * later, and do not call iterate()/reset()/restart())/set_source() and
 * any fricking thing that will mutate statement, that keeps theese
 * values, because they may be freed and you will get USE AFTER FREE = UB!
 */
struct Text;
struct Blob;

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
 * @brief Array of compiled statements.
 * They are all precompiled early.
 * Pros :
 * - Very fast, if Program is reused and thus it's not recompiled from scratch every single time.
 * - No need to create copy of entire SQL source string (it is done internally by sqlite and only for important parts)
 * Cons :
 * - May take more memory than statement-by-statement compile()/execute().
 * - may be pointless if you don't give a fuck about reusing existent program and recompile it over and over again.
 * follows rules and warnings of the Query class. See below.
 *
 * @warning do NOT add and use after CREATE TABLE/VIEW/TRIGGER statements here! 
 * because compilation comes before the evaluation, statements after CREATE will NOT
 * find required table and fail to compile => fail to compile an entire program.
 * Good rule is to place all CREATE/PRAGMA and other statements that modifies SCHEMA
 * in the Query right after opening the database!
 */
class Program;

/** 
 * @brief Query is an SQL statements interpreter.
 * it just works, but may be not the bset option.
 * statements are compiled one by one, like in sqlite_exec().
 * Prons :
 * - Less memory overhead.
 * Cons :
 * - High overhead on recompilation if called in hot code paths...
 * - More errors may happen IN the execution process (generally - 
 * syntax errors), and it may leave some thansaction incompleted!
 *
 * @warning it does not attempts to do any rollback/commit, because it's your
 * responcibility. Program did absolutely similar thing (no commit/rollback),
 * but it may fail only on very strong errors or SQLITE_BUSY....
 *
 * Also, both Program and Query do rewind() back to the first statement on errors.
 * This is intencional. You SHOULD not ignore such errors, and most of simple
 * errors like "table already exist" may be prevented directly in SQL code. 
 * Attempt to "continue no matter what" can lead to hardly-detectable bugs in your code.
 * That's why this option is NOT awailable.
 */
class Query;

/**
* @brief RAII Database wrapper. 
* 
* @warning You must destroy all objects that uses it before destructing/closing database.
* This includes Statement, Query, Program and Backup! 
*/
class Database;

/** Optional error, returned by most of the functions. */
class DatabaseError;

/** Generic exception, related to raiisqlite module */
class DatabaseException;

/** 
 * @brief Step-by-step database backup routine
 * @warning both databases, passed in constructor, must exists and not change
 * while Backup routine is not finished.
 *
 * Not changed means not reopened and not closed. You may still use
 * source database in breaks between iterate() calls... (but not dest)
 */
class Backup;

using Byte = uint8_t;

/** callback to retrieve data on SQLITE_ROW */
using ColumnCallback = std::function<void(QueryResult res)>;

};	// namespace sqlite

/// definition
namespace sqlite {

/** If enabled (default) any unhandled(unchecked) Database 
 * Error will print to stderr about being unhandled!
 */
static constexpr bool REPORT_UNHANDLED_ERRORS = true;

class DatabaseError {
	friend class Testing;
	int errcode = 0;
	bool is_error : 1;
	bool is_checked : 1;
	public:
	inline DatabaseError() {is_error = 0; is_checked = 0;}
	inline DatabaseError(int sqlerr) {
		set_error(sqlerr);
	}
	public:

	// construction reqires recheck
	inline DatabaseError(DatabaseError&& src) {
		*this = std::move(src);
		is_checked = 0;
	}

	/** set error */
	inline void set_error(int sqlerr) {
		errcode = sqlerr;
		is_checked = 0;
		int is_ok = 0;
		is_ok |= (sqlerr == SQLITE_OK);
		is_ok |= (sqlerr == SQLITE_DONE);
		is_ok |= (sqlerr == SQLITE_ROW);
		is_error = !is_ok; 
	}

	/** copy error and errstatus, and set is_checked on source */
	DatabaseError& operator=(DatabaseError&& src) {
		errcode = src.errcode;
		is_checked = src.is_checked;
		src.is_checked = 1;
		is_error = src.is_error;
		return *this;
	}

	bool check() {is_checked = 1; return !is_error;} // error is checked
	int get() {is_checked = 1; return errcode;} // error is kinda checked?
	void supress() {is_checked = 1;} // threat as we don't care about this error at all

	// to prevent misuse. use get() method for that!
	operator int() = delete;

	// convinient operators
	inline bool operator==(int sqlerr) {
		is_checked = 1;
		return errcode == sqlerr;
	}
	inline bool operator!=(int sqlerr) {return !(*this == sqlerr);}

	inline ~DatabaseError() {
		if constexpr (REPORT_UNHANDLED_ERRORS) {
			if (is_error && !is_checked)
				fprintf(stderr, "unhandled Database Error : %s, %i\n", sqlite3_errstr(errcode), errcode);
		}
	}

};

class DatabaseException : public std::runtime_error {
 public:
	friend class Testing;
	using re = std::runtime_error;
	using re::re;
	int errcode = 0;
	DatabaseException(const char *msg, int err) : std::runtime_error(msg), errcode(err) {}
	DatabaseException(std::string msg, int err) : std::runtime_error(msg), errcode(err) {}
};

struct Text {
	const char *src = nullptr;
	size_t size = 0;
	bool is_static = false;

	Text(const char *s, size_t l, bool nocopy = false) : src(s), size(l), is_static(nocopy) {}
	Text(const std::string_view s, bool nocopy = false) : src(s.data()), size(s.size()), is_static(nocopy) {}
	Text(const std::string &s, bool nocopy = false) : src(s.data()), size(s.size()), is_static(nocopy) {}

	/** @warning uses strlen() to compute string length! */
	Text(const char *s, bool nocopy = false);

	Text(const Text &src) = default;

	operator std::string() { return {src, size}; }
	operator std::string_view() { return {src, size}; }
};

struct Blob : public Text {
	Blob(const void *s, size_t l, bool nocopy = false) : Text((const char *)s, l, nocopy) {}
	Blob(const std::vector<Byte> &s, bool nocopy = false) : Text((const char *)s.data(), s.size(), nocopy) {}
	operator std::vector<Byte>() { return {src, src + size}; }
};

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

_TMP_MAGIC_SHIT(Text, text, v.src, v.size, v.is_static ? SQLITE_STATIC : SQLITE_TRANSIENT);
_TMP_MAGIC_SHIT(Blob, blob, v.src, v.size, v.is_static ? SQLITE_STATIC : SQLITE_TRANSIENT);

#undef _TMP_MAGIC_SHIT

/** raw column */
template <typename T>
inline T sqlite_column(sqlite3_stmt *, int) {
	static_assert("not implemented!");
}

template <>
inline std::nullptr_t sqlite_column<std::nullptr_t>(sqlite3_stmt *, int) {
	return nullptr;
}

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
	return {(char *)sqlite3_column_text(stmt, idx), (size_t)sqlite3_column_bytes(stmt, idx)};
}

template <>
inline Blob sqlite_column<Blob>(sqlite3_stmt *stmt, int idx) {
	return Blob(sqlite3_column_blob(stmt, idx), (size_t)sqlite3_column_bytes(stmt, idx));
}

/** general cases */
template <>
inline std::string_view sqlite_column<std::string_view>(sqlite3_stmt *stmt, int idx) {
	return sqlite_column<Text>(stmt, idx);
}
template <>
inline std::string sqlite_column<std::string>(sqlite3_stmt *stmt, int idx) {
	return sqlite_column<Text>(stmt, idx);
}
template <>
inline std::vector<Byte> sqlite_column<std::vector<Byte>>(sqlite3_stmt *stmt, int idx) {
	return sqlite_column<Blob>(stmt, idx);
}

};	// namespace impl

class QueryResult {
	friend class Testing;
 protected:
	friend class Statement;
	sqlite3_stmt *stmt;
	QueryResult(sqlite3_stmt *v) : stmt(v) {
		if (v == nullptr) throw DatabaseException("nullptr statement!", 0);
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

	/** creates statement from specified SQL code. Will set leftover pointer to the rest of not used sql code.
	 * also accepts SQLITE_PREPARE_* flags
	 * it will ignore empty spaces in SQL code, however, will return
	 * @return SQLITE_OK or error code (SQLITE_EMPTY for no code).
	 * @warning this operation ALWAYS destroys previous statement (and invalidates
	 * old statement pointer!)
	 *
	 * you may get compilation error using get_compile_error() method, HOWEVER, it have some limitations.
	 * see @ref get_compile_error() docs for more info.
	 */
	DatabaseError compile(sqlite3 *db, Text src, const char **leftover = nullptr, int flags = 0) noexcept;

	/** Executes statement.
	 * returns SQLITE_DONE => execution is complete successfully, SQLITE_ROW => we have result data to process, any other value => error.
	 */
	DatabaseError iterate() noexcept;

	/** get results from statetment.
	 * You can only call this when iterate() returns SQLITE_ROW!
	 * @warning see warnings in QueryResult class documentartion!
	 */
	inline QueryResult result() const {
		if (!stmt) throw DatabaseException("cannot retrieve data from an empty statement");
		return QueryResult(stmt);
	}

	/** resets statement executon position.
	 * you may call iterate() again after that :)
	 * but you shouldn't call this AFTER iterate()!
	 * internally iterate always resets statement when it
	 * finishes ar returns ERROR! 
	 */
	inline void reset() noexcept {
		if (stmt) sqlite3_reset(stmt);
	}

	/** just do it, again (c)
	 * does exact the same as ```
	 * DatabaseError err;
	 *
	 * while ((err = stmt.iterate()) == SQLITE_ROW) {
	 *	cb(stmt.result());
	 * }
	 * return err;
	 * ``` with an additional reset() and break on exception
	 *
	 * you may break execution by throwing std::exception.
	 * but be careful, your exception will not be rethrown!
	 */
	DatabaseError execute(ColumnCallback cb = nullptr) noexcept;

	/** bind arguments... some black magic, ya? */
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

	bool is_busy() {return sqlite3_stmt_busy(stmt);}

	/** for debugging purposes. */
	std::string expanded_sql();

	/** pretty error ouput
	  * @warning SQLITE keeps compilation error message instde database handle.
		* this means that you will not get valid error message if you will trigger
		* any other error after compile error and between call to this function!
		*
		* db is a database, passed in compile() routine.
		* Text should be original SQL code, passed to compile() routine, and
		* leftover should be pointer, untouched by you, that was passed to compile() too.
		*
		* returns empty string on error. Or empty error message.
		*/
	std::string get_compile_error(sqlite3* db, Text src, const char * const* leftover) noexcept;

	operator sqlite3_stmt *() noexcept { return stmt; }

	~Statement() { release(); }
};

class Backup {
 friend class Testing;
 protected:
	sqlite3_backup *ctx = nullptr;
	int sleep_amount = 100;
 public:
	Backup(const Backup &) = delete;
	Backup(Backup &&) = default;
	Backup &operator=(const Backup &) = delete;
	Backup &operator=(Backup &&) = default;
	Backup() = default;

	template <typename... Args>
	inline Backup(sqlite3 *src, sqlite3 *dest, Args&&... args) {
		if (!src || !dest) throw DatabaseException("database is nullptr!");
		if (!start(src, dest, std::forward<Args>(args)...).check()) {
			throw DatabaseException(sqlite3_errmsg(dest));
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
	 */
	DatabaseError start(sqlite3 *src, sqlite3 *dest, int delay=25, const char* schema_name = "main") {
		if (!src || !dest) throw DatabaseException("database is nullptr!");
		if (!schema_name) schema_name = "main";
		ctx = sqlite3_backup_init(dest, schema_name, src, schema_name);
		if (!ctx) {
			return DatabaseError(sqlite3_errcode(dest));
		}
		sleep_amount = delay > 0 ? delay : 0;
		return DatabaseError(); // OK
	}

	int remaining() const noexcept { return ctx ? sqlite3_backup_remaining(ctx) : 0; }
	int length() const noexcept { return ctx? sqlite3_backup_pagecount(ctx) : 0; }
	int position() const noexcept { return length() - remaining(); }

	// wait to allow db to work a bit in background.
	void sleep() noexcept {
		if (sleep_amount > 0)
		sqlite3_sleep(sleep_amount);
	}

	/** may return error */
	DatabaseError destroy() {
		if (!ctx) return SQLITE_EMPTY;

		/* Release resources allocated by backup_init(). */
		int err = sqlite3_backup_finish(ctx);
		ctx = nullptr;
		return err;
	}

	/** iterator. returns SQLITE_*** codes.
	 * you should explicitly handle SQLITE_BUSY and SQLITE_LOCKED errors
	 * SQLITE_DONE and SQLITE_OK notifies about SUCCESS!
	 * does not throw exceptions
	 */
	DatabaseError iterate(int n_pages = 10) noexcept;

	/** differennt backup executors.
	 * ths function will return err if SQLITE_BUSY/SQLITE_LOCKED
	 * error is recieved, to give application opportunity
	 * to make a desicion. Sleeps between steps.
	 * throws on other errors
	 * returns SQLITE_DONE on success;
	 *
	 * @example ```
	 * DatabaseError err;
	 * while ((err = backup.execute_until_busy()) != SQLITE_DONE) {
	 *  // do some stuff while we are busy!
	 * }
	 * ```
	 */
	DatabaseError execute_until_busy(int n_pages = 10);

	/** differennt backup executors.
	 * this one assumes that src database will never
	 * be busy for too long (or busy at all).
	 * throws on errors and SQLITE_LOCKED!
	 */
	void execute_sync(int n_pages = 10);

	/** different backup executors.
	 * executes backup procedure without delays and NOW!
	 * assumes that src database will never be busy for too long (or busy at all).
	 * Throws on errors.
	 */
	void execute_now();

	/// rolls back everything if backup was not finished!
	~Backup() { destroy().supress(); }
};

namespace impl {

/** @interface description of QueryStorage Base class
 *
 * constructor() - takes database as argument.
 * get() - returns current statement, if available. 
 *  should return statement after rewind() or compile()! 
 *  Return nullptr at end or error!
 * release() - self explainotary...
 * database field MUST be acessable.
 *
 * get_error() - returns std::string_view with an error message!
 * may return empty get_error(). 
 *
 * All functions below return SQLITE_OK on successs and errors in case of error, all noexcept!
 * compile() - compile or save for later sql code + compile at least 1 statement.
 * rewind() - rewinds back to the first statement + reset current executed.
 * next() - goto next statement. return SQLITE_DONE on END or return error if error.
 *  Old statement may be invalidated after this.
 */

/** there is only one statement built at the time. SQL code is copied!
 * does not check for errors on compile, but at execution time.
 */
class InterpretQueryStorage {
 friend class sqlite::Testing;
 protected:
 friend class sqlite::Testing;
	Statement stmt;
	std::string tmpbuff; // tmpbuffer for sql
	std::string errmsg; // error message

	std::string_view sql;				 // on std::string SQL
	const char *cpos = nullptr;	 // position in code

	Text curr() { return Text(cpos, (size_t)(sql.end() - cpos)); }

 public:
	sqlite3 *database = nullptr;

	InterpretQueryStorage(InterpretQueryStorage &&) = default;
	InterpretQueryStorage(sqlite3 *db) : database(db) {
		if (!db) throw nullptr;
	}

 public:
	// called in iterate() and here to build an error message
	void make_errmsg() {
		errmsg = stmt.get_compile_error(database, sql, &cpos);
	}

	DatabaseError rewind() noexcept;

	DatabaseError compile(Text sql_code) noexcept {
		tmpbuff = sql_code; // make a copy of it!
		sql = tmpbuff;
		return rewind();
	}

	DatabaseError next() noexcept;

	void release() noexcept {
		stmt.release();
		errmsg.clear();
		sql = std::string_view();
	}

	Statement *get() noexcept { return stmt ? &stmt : nullptr; }

	std::string get_error() {return errmsg;}
};

/** Static query storage. Precompiles whole SQL source into the vector of statements. 
  * has no beautiful error messages at execution time :)
	*/
class StaticQueryStorage {
 friend class sqlite::Testing;
 public:
 // for v.1.1 bind_all()! DO NOT MESS AROUND WITH IT!
 std::vector<Statement> statements;
 protected:
	int index;
	std::string errmsg;

	Statement *curr() { return index < (int)statements.size() ? &statements[index] : nullptr; }

 public:
	sqlite3 *database = nullptr;

	StaticQueryStorage(StaticQueryStorage &&) = default;
	StaticQueryStorage(sqlite3 *db) : database(db) {
		if (!db) throw nullptr;
	}

 public:
	DatabaseError rewind() noexcept {
		if (curr()) {
			curr()->reset();
		}
		index = 0;
		// empty code is an error!
		return curr() ? SQLITE_OK : SQLITE_EMPTY;
	}

	DatabaseError compile(Text sql_code) noexcept;

	void release() noexcept {
		statements.clear();
		index = 0;
		errmsg.clear();
	}

	DatabaseError next() noexcept {
		if (!curr()) return false;
		curr()->reset();
		index++;
		return curr() ? SQLITE_OK : SQLITE_DONE;
	}

	Statement *get() { return curr(); }

	void make_errmsg() {
		errmsg = "#"; errmsg += index; errmsg += " : ";
		errmsg += sqlite3_errmsg(database);
	}
	std::string get_error() {return errmsg;}
};

template <class QueryStorage>
class BasicQuery {
	friend class sqlite::Testing;
 protected:
	QueryStorage storage;	 // implementation is here :p

	/** restart at error - don't override error message! */
	void restart_and_throw(DatabaseError& e) {
		std::string msg = storage.get_error();
		if (storage.rewind() != SQLITE_OK) {
			msg += "\nFailed to restart() properly! This Query/Statement will be unusable!";
		};
		throw DatabaseException(msg, e.get());
	}

 public:
	BasicQuery(sqlite3 *db) : storage(db) {}

	/** release current statement, if any */
	void release() { storage.release(); }

	/** throws on error */
	void set_code(Text src) {
		release();
		if (storage.compile(src).get() != SQLITE_OK) // compile error
			throw DatabaseException(storage.get_error());
	}

	/** returns SQL code compilation/execution error. */
	std::string get_error() {return storage.get_error();}

	operator bool() { return !!storage.get(); }

	/** bind arguments */
	template <typename... Args>
	void bind(Args &&...args) {
		Statement *stmt = storage.get();
		if (stmt) stmt->bind(std::forward(args)...);
	}

	void restart() {
		DatabaseError e;
		if ((e=storage.rewind()) != SQLITE_OK) {
			std::string s = "Unable to restart()! : ";
			s += storage.get_error();
			throw DatabaseException(s, e.get());
		}
		if (storage.get() == nullptr) // unlikely
			throw DatabaseException("empty statement : malfunction");
	}

	/** execute current statement. you should call it again if returns true. throws on
	 * error. Rewinds back to the start on error! 
	 * @warning unlike execute() it does not restarts execution!
	 * you will get an execption if you will try to iterate() over completed query/program.
	 * call restart() to reexecute program again :)  
	 */
	bool iterate(ColumnCallback cb = nullptr) {
		Statement *stmt = storage.get();
		if (!stmt) throw DatabaseException("iteration over an empty statement!");

		DatabaseError rc = stmt->execute(cb);

		// execution error
		if (rc != SQLITE_DONE && rc != SQLITE_ABORT) {
			storage.make_errmsg(); // record error (storage does not handle execution!)
			restart_and_throw(rc); // yep
		}

		// next statement
		rc = std::move(storage.next()); 
		if (rc == SQLITE_DONE) return false; // end

		// compilation or switching error
		if (rc != SQLITE_OK) {
			restart_and_throw(rc); // we should hae an errormesage here!
		}

		return true; // should execute next statement
	}

	/** just execute it (c)
	 * calls restart() before iteration :)
	 */
	template <typename... Args>
	void execute(ColumnCallback cb = nullptr, Args &&...args) {
		bool stat = 0;
		restart();

		do {
			Statement *stmt = storage.get();
			if (stmt) stmt->bind(std::forward<Args>(args)...);
			stat = iterate(cb);
		} while (stat > 0);
	}
};

};	// namespace impl

class Program: public impl::BasicQuery<impl::StaticQueryStorage> {
	friend class Testing;
 public:
	using Base = impl::BasicQuery<impl::StaticQueryStorage>;
	using Base::Base;

	/// v 1.1 extension. Ability to bind all parameters for all statements early,
	/// without requirement to execute() first!
	/// does not resets() program or anything! exclusive to Program class!
	template <typename... Args>
	void bind_all(Args &&...args) {
		for (auto& stmt : storage.statements) {
			if (stmt) stmt.bind(std::forward<Args>(args)...);
		}
	}

	/// v.1.1 extension. Ability to unbind() all parameters from all statements.
	/// follows bind_all() properties!
	void unbind_all() {
		for (auto& stmt : storage.statements) {
			if (stmt) stmt.unbind();
		}
	}
};

class Query : public impl::BasicQuery<impl::InterpretQueryStorage> {
	friend class Testing;
 public:
	using Base = impl::BasicQuery<impl::InterpretQueryStorage>;
	using Base::Base;
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
 *
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
	DatabaseError raw_open(const char *path, int flags) noexcept;

	/// same as sqlite::connect_or_create()!
	void open(const char* path) {*this = sqlite::connect_or_create(path);}

	/// same as sqlite::connect_uri()!
	void open_uri(const char* uri) {*this = sqlite::connect_uri(uri);}

	/** flushes all cache into disk to write results. */
	void flush() {
		if (db) sqlite3_db_cacheflush(db);
	}

	/** attempts to free as much memory as possible. Use in case of OOM */
	void shrink_to_fit() {
		if (db) sqlite3_db_release_memory(db);
	}

	/** check is database readonly. result on closed/not opened database is undefined. */
	bool is_readonly() { return db ? sqlite3_db_readonly(db, "main") : false; }

	/** fire and forget. sqlite3_exec()-like, but with bind() available. throws on error! */
	template <typename... Args>
	inline void exec(Text sql, Args&& ...args) {
		if (!db) throw DatabaseException("database is not opened!");
		Query query(db);
		query.set_code(sql);
		query.execute(nullptr, std::forward<Args>(args)...);
	};

	/** fire and forget. sqlite3_exec()-like, but with bind() available. throws on error! */
	template <typename... Args>
	inline void query(Text sql, ColumnCallback cb, Args &&...args) {
		if (!db) throw DatabaseException("database is not opened!");
		Query query(db);
		query.set_code(sql);
		query.execute(cb, std::forward<Args>(args)...);
	};

	/** compile Program to be executed 
	 * @warning read Program docs for more info!
	 */
	Program compile(Text sql) {
		if (!db) throw DatabaseException("database is not opened!");
		Program res(db);
		res.set_code(sql);
		return res;
	}

	/// v.1.1 extension. Bind arguments early, without execute()!
	template <typename... Args>
	inline Program compile(Text sql, Args&& ...args) {
		Program res = compile(sql);
		res.bind_all(std::forward<Args>(args)...);
		return res;
	}

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
 */
inline Database connect_or_create(const char* path) {
	return connect(path, false, true);
}

/** opens in-memory database. No data will be readed/writed from/to disk!
 */
Database create_memory(const char* path = "");

};
