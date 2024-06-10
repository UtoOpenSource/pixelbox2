/*
 * This file is a part of Pixelbox - Infinite 2D sandbox game
 * Execution time profiler
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

/**
 #define PROFILER_DEFINE_EXT to get extera functions, SPECIFICALLY to retrieve profiling stats
*/

#pragma once

#include <cstdlib>
#include <thread> // for ThreadID

#ifdef PROFILER_DEFINE_EXT
#include <vector>
#include <map>
#endif

namespace pb {
namespace prof {

/**
 * Please, don't forget, that
 * profiling is still NOT FREE, and using it just in any small
 * function like std::vector::get() is pretty useless and not worth it!
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

	/**
	 * Push new profiler zone call on the stack.
	 * NEW REQUIREMENT : All nemes passed to name must be very unique.
	 * NEW ISSUE : Any unique passed string will be keeped in memory FOREVER!
	 * : This is not a good idea to let unsafe enviroment use this function!
	 * Also, zone stack has implementation-defined boundary limit... yep
	 */
	void begin(const char* name);

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
	 * done.~~ This requirement is removed in current version : you can safely
	 * tick in the middle of deep zone stack :)
	 */
	void step();

	/** syntax sugar and safer wrapper of begin()/end() functions above
	   * Calls begin() immediatly, and calls end when returned object is destructed
		 */
  auto make_zone(const char* name) {
    begin(name);
		struct __nnn {
			ThreadData &master;
			__nnn(ThreadData& d) : master(d) {}
			~__nnn() {master.end();}
		}v (*this);
	  return v;
  }
};

/** get ThreadData for current thread. CANNOT BE USED BEFORE INIT OR AFTER FREE!
 * this thing is likely to be cached in a thread_local variable, so it's ok
 * in terms of perfomance.
 */
ThreadData get_thread_data();

/**
 * Every thread, that will use this profiler, MUST be registered ONCE AND properly unregistred!
 * May throw exceptions...
 *
 * Unfortunately, there is NO portable way to attach finalizer to any thread :(
 */
ThreadData init_thread_data();
void       free_thread_data(ThreadData);

#ifdef PROFILER_DEFINE_EXT

/** convinient wrapper around initialization things above */
inline auto make_thread_data() {
	ThreadData data = init_thread_data();
	struct __nnz {
		ThreadData &master;
		__nnz(ThreadData& d) : master(d) {}
		~__nnz() {free_thread_data(master);}
	}v (data);
	return v;
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
	float sumtime = 0;	// how long this entry and all called subentries are
									// executed
	int ncalls = 0;	// number of "calls" to this entry
};

/** this is how entry/zone is stored internally */
using StatsStorage2 = std::map<const std::string*, prof_stats>;
using ThreadID = std::thread::id;

/** return vector of all threads */
void get_threads(std::vector<ThreadID>&);

/** return length of history buffers (or (max position+1) in history/summary) */
size_t history_size();

/**
 * Get statictics about all zones in one thread at specified position in time.
 * Calling this every frame is somewhat costly, but
 * it allows us to do minimal syncronisation possible :)
 * 
 * Returns no value if thread with this ID does not exist.
 * pos is in range of 0 to history_size()-1;
 */
StatsStorage2 get_summary(ThreadID, size_t pos);

/**
 * Returns "current" position in history for this thread.
 */
size_t get_current_position(ThreadID id);
#endif

};
}

/*
 * MACRO MAGIC AT THE END
 */
#include "engine/base.hpp"
#define PROFILING_SCOPE_X(name, ctx) \
auto ANONYMOUS(_PROF_ZONE_N) = ctx.make_zone(name);

#define PROFILING_SCOPE(name) \
auto ANONYMOUS(_PROF_ZONE_N) = pb::prof::get_thread_data().make_zone(name);
