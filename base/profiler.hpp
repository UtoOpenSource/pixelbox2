/*
 * This file is a part of Pixelbox - Infinite 2D sandbox game
 * Execution time ~~and memory~~ profiler
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
#include "base/base.hpp"
#include "base/scopeguard.hpp"

#include <map> // insertion speed and reference stablility matters here
#include <thread> // for ThreadID
#include <vector>

namespace pb {
namespace prof {

/**
 * Please, don't forget, that
 * profiling is still NOT FREE, and using it just in any small
 * function like std::vector::get() is pretty useless and worth it!
 *
 * Apply profiling only for significant algoritms, like garbage
 * collection, physic update, game objects update, sorting of game
 * objetcs for proper Z order drawing, mesh building, terrain
 * generation and etc.
 */

// implementation of very presize and stable clocksource
// DON'T USE A DEFAULT clock() FUNCTION FROM LIBC! IT'S AWFUL!
// I am using my own clocksource here.
double prof_clock();

/** how data is really implmented... */
struct DataImpl;

/**
 * Thread-specific profiler context
 */
class ThreadData {
	friend struct DataImpl;
	public:
	DataImpl& data;
	private:
	ThreadData(DataImpl& src) : data(src) {}
	public:
	ThreadData(const ThreadData &) = default;
	ThreadData(ThreadData &&) = default;
	public:
	/* API */

	/** Push new profiler zone call on the stack */
	void begin(const HString& name);
	/** end of call to previously pushed zone. Return to previous zone */
	void end();
	/*
 	 * Call this at the end of the thread tick, or just to finally write
 	 * results to the history.
 	 *
	 * Remember about floating point presizion, don't make pauses between
 	 * this very long.
	 *
	 * ~~THIS FUNCTION REQUIRES FOR ALL PUSHED ENTRIES TO BE POPPED OUT!~~
	 * ~~Aka, this function MAY be ONLY called after all profiling zones are
	 * done.~~ This requirement is removed in current version : you can freely
	 * tick in the middle of deep zone stack :)
	 */
	 void step();

	 /** syntax sugar and safer wrapper of begin()/end() functions above
	   * Calls begin() immediatly, and calls end when returned object is destructed
		 */
	 pb::ScopeGuard<void()> make_zone(const HString& name);
};

/**
 * Every thread, that will use this profiler, MUST be registered ONCE AND properly unregistred!
 * May throw exceptions...
 */
ThreadData init_thread_data();
void       free_thread_data(ThreadData);

/** get ThreadData for current thread. CANNOT BE USED BEFORE INIT OR AFTER FREE! */
ThreadData get_thread_data();

/** convinient wrapper around initialization things above */
inline pb::ScopeGuard<void()> make_thread_data() {
	ThreadData data = init_thread_data();
	return pb::ScopeGuard<void()> ([data]() {
		free_thread_data(data);
	});
}

/*
 * the SUMMARY time of every entry/zone SHOULD be equal to sum of it's own
 * execution time AND the summary time of subentries OR the own time
 * OF ALL SUBENTRIES AND ALL SUBENTRIES OF SUBENTRIES AND ETC.
 *
 * Summary time basically means how long this entry was a deal, in any
 * case. Own (execution) time means only time, while only this entry
 * itself was running, not any subentries.
 */
struct prof_stats {
	float owntime = 0;	// how long this entry was executed (subentries are
									// EXCLUDED)
	float sumtime = 0;	// how long this entry and all CALLED subentries are
									// executed
	int ncalls = 0;	// number of "calls" to this entry
};

/** this is how entry/zone is stored internally */
using StatsStorage = std::map<pb::HString, prof_stats>;
using ThreadID = std::thread::id;

/** return vector of all threads */
std::vector<ThreadID> get_threads();

/** return length of history buffers (or (max position+1) in history/summary) */
size_t history_size();

/**
 * Get statictics about all zones in one thread at specified position in time.
 * returns whole ass copy of everything.
 * Calling this every frame is absolutely costly, but
 * it allows us to do minimal syncronisation possible :)
 * 
 * Returns no value if thread with this ID does not exist.
 * pos is in range of 0 to history_size()-1;
 */
StatsStorage get_summary(ThreadID, size_t pos);

};
}

/*
 * MACRO MAGIC AT THE END
 */

#define PROFILING_SCOPE(name, ctx) \
auto ANONYMOUS(_PROF_ZONE_N) = ctx.make_zone(name);
