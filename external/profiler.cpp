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

#define PROFILER_DEFINE_EXT
#include "profiler.hpp"
#include "clock.hpp"

#include <stdexcept>
#include <thread>

#include <stack>
#include <utility>
#include <stdexcept>
#include <unordered_map>
#include <mutex>

#include "engine/hashmap.hpp"
#include <doctest.h>

namespace pb {

namespace prof {

using Thread = std::thread;
using StatsStorage = std::map<const std::string*, prof_stats>; // REAL stats storage
static constexpr int HISTORY_LEN = 128; // how many history entries to keep

double prof_clock() {
	return ClockSource().time();
}

/** zone item on the stack */
struct prof_item {
	prof_item(prof_stats* val, double time) : val(val), time(time) {}
	prof_item(const prof_item&) = default;
	prof_item(prof_item&&) = default;
	prof_item& operator=(const prof_item&) = default;
	prof_item& operator=(prof_item&&) = default;
	public:
	prof_stats* val;	// where to write a results
	double time;
};

namespace impl {
	static ThreadID curr_id() {return ::std::this_thread::get_id();}	

	class Stack : public std::stack<prof_item> {
		using C = std::stack<prof_item>;
		using C::C;
		public:
		C::container_type& container(){
			return c;
		}
	};
};

/**

 resource and other shared thread stuff...
 
  */
template <class T, class Mutex = std::mutex>
struct ResUsage {
	std::unique_lock<Mutex> lock;
	T& ref;
	public:
	operator T& () {return ref;}
};

template<class T, class Mutex = std::mutex>
class Resource : public T{
	private:
	Mutex m;
	//T object;
	public:
	template<class... Args>
	Resource(Args&&... args) : T(std::forward<Args>(args)...) {}
	ResUsage<T, Mutex> use() {
		return {std::unique_lock<Mutex>(m), static_cast<T&>(*this)};
	}
};

/*
 * For debug. Turns resource aqquring and releaseing into NOOP
 */
template <class T>
struct ResUsage<T, void> {
	T& ref;
	public:
	operator T& () {return ref;}
};

template<class T>
class Resource<T, void> : public T{
	private:
	//T object;
	public:
	template<class... Args>
	Resource(Args&&... args) : T(std::forward<Args>(args)...) {}
	ResUsage<T, void> use() {
		return {static_cast<T&>(*this)};
	}
};


/** per-thread profiler data. We assume that only owning thread will access this data. */
struct DataImpl {
	public:
	const ThreadID key;
	impl::Stack stack;
	StatsStorage data;
	int history_pos = 0; // position in the history
	double tick_time; // when was ticked last time. if > 5 seconds, we can remove thread.
	public:
	DataImpl(ThreadID id) : key(id) {use();}
	DataImpl(const DataImpl&) = default;
	DataImpl(DataImpl&&) = default;
	private:
	double use() {
		return prof_clock();
	}
	/** get statictics for a given zone name,
	  * or create new one */
	prof_stats* stat(const std::string* name) {
		return &(data.emplace(name, prof_stats()).first)->second;
	}
	/** push zone call on proflier stack */
	prof_item& push(const std::string* name) {
		prof_stats* val = stat(name);
		auto& v = stack.emplace(val, use());
		if (stack.size() == 0) std::runtime_error("What the fuck");
		return v;
	}
	/** pop zone call from stack*/
	inline prof_item pop() {
		prof_item i = stack.top();
		stack.pop();
		return i;
	}
	/** get zone call on top of the stack */
	inline prof_item& get() {
		return stack.top();
	}
	/** is there any zone call? */
	inline bool have() {
		return !stack.empty();
	}
	public:
	ThreadData get_wrapper() {
		return ThreadData(*this);
	}
	/** API */
	void begin(const std::string* name);
	void end();
	void step();
	/** private extended */
	void newframe();
};

/**
SPINLOCK
\*/

	// use ONLY under very active load, and short lock period!
	class SpinLock {
		std::atomic_bool lo;
		using Lock = std::unique_lock<SpinLock>;
		public:
		inline bool try_lock() {
			bool unlocked = false;
			// invalidate cache here
			return lo.compare_exchange_weak(
				unlocked, true, std::memory_order_acquire
			);
		}
		inline void lock() {
			while (!try_lock()) { // invalidates cache
				// a bit more OK
				while(lo.load(std::memory_order_acquire) != false) {}
			}
		}
		inline void unlock() {
			lo.store(false, std::memory_order_release);
		}
	};

/*
 * API Implementation
 */
namespace impl {
	static Resource<std::map<ThreadID, DataImpl>, SpinLock> prof_data;
	using StatsHistory = std::vector<StatsStorage>;
	using HistoryMap = std::unordered_map<ThreadID, StatsHistory>;
	static Resource<HistoryMap, void> prof_history;
	static Resource<pb::HashSet<std::string>, SpinLock> string_cache;

	thread_local DataImpl* _data_ref = nullptr;

	DataImpl& get_thread_data() {
		if (_data_ref) return *_data_ref; // fast path

		auto id = curr_id();
		auto ruse = prof_data.use(); // lock
		auto& data = ruse.ref;

		auto v = data.find(id); // we don't want to always call a constructor
		if (v != data.end()) {
			_data_ref = &v->second;
			return v->second;
		}

		throw std::runtime_error("thread was not registered!");
	};

	DataImpl& new_thread_data() {
		auto id = curr_id();
		auto ruse = prof_data.use(); // lock
		auto& data = ruse.ref;

		auto v = data.find(id); // we don't want to always call a constructor
		if (v != data.end()) throw std::runtime_error("thread was already registered!");;

		return data.emplace(id, DataImpl(curr_id())).first->second; // else
	};

	void delete_thread_data(DataImpl& impl) {
		auto id = curr_id();
		if (id != impl.key) std::runtime_error("Deleting thread data from another thread. \
		(If you don't change anything, fault is in your stdc++ library std::thread destructor implementation)");

		auto ruse = prof_data.use(); // lock
		auto& data = ruse.ref;

		auto v = data.find(id); // we don't want to always call a constructor
		if (v == data.end()) std::runtime_error("This thread was already unregistered. You have big and bad issues... Use sanitizers");
		data.erase(v);
	}

	/** HISTORY MUST BE LOCKED! */
	StatsHistory& get_history_data(ThreadID key, HistoryMap& history) {
		auto it = history.find(key);
		if (it == history.end()) return history.emplace(key, StatsHistory(HISTORY_LEN)).first->second;
		return it->second;
	}

};

/* How does it work?
 * Every time we push new profiler scope(entry), we set
 * summary and own time for previous as difference of
 * the current time and previously saved in previous entry.
 * + we set time for previous entry as current. Any write interaction
 * with any entry requires to do that.
 *
 * When we pop scope(entry), we will do similar stuff with popped
 * element, BUT also we will ADD time difference for the current
 * (after pop) element, using it's own old time.
 * NOT OWN TIME! only summary!
 *
 * also we count number of "calls" - pushes of the profiler scopes.
 *
 * When prof_step() is called, all current processed data is pushed
 * into the profiler history and cleaned up after that.
 *
 * All this technique is minded by me, on paper, in one day. It
 * may work very ugly, but it works, and i don't need more :p
 *
 * NEW: String passed here MUST be a string in PROFILER STRING CACHE!
 */

void DataImpl::begin(const std::string* name) {
	double time = prof_clock();

	// set owntime and sumtime for previous entry
	if (have()) {
		prof_item& prev = get();
		prof_stats& s = *prev.val;

		s.owntime += time - prev.time;
		s.sumtime += time - prev.time;
		prev.time = time;
	}

	push(name);
	stat(name)->ncalls++; // new call
}

void DataImpl::end() {
	double time = prof_clock();

	prof_item item = pop();
	prof_stats& stat = *item.val;

	stat.owntime += time - item.time;
	stat.sumtime += time - item.time;

	if (have()) {	// add to summary time of the current top item
		prof_item& prev = get();
		auto &stat = *prev.val;
		stat.sumtime += time - prev.time;
		prev.time = time;
	}
}

/**
 * Clearups time in zones stack and zone data
 */
void DataImpl::newframe() {
	auto& vec = stack.container();
	double time = use();

	for (auto &i : stack.container() ) {
		i.time = time; // reset time flor all zones on the stack
	}

	// erase data
	std::erase_if(data, [&vec](const auto& pair){
		for (auto& i : vec) {
			if (i.val == &(pair.second)) return false; // do not erase
		}
		return true; // do erase
	});

	// clearup
	for (auto &pair: data) {
		prof_stats& v = pair.second;
		v.ncalls = 1; // this shit is already called
		v.owntime = 0;
		v.sumtime = 0;
	}
}

/**
 * AWARE OF NEW REQUIREMENT! We need to restore stack as it was before, if it is not empty!
 */
void DataImpl::step() {
	{
	auto ruse = impl::prof_history.use(); // lock
	auto& history_map = ruse.ref;

	auto& history = impl::get_history_data(key, history_map);
	auto &dst = history[history_pos];
	dst.clear(); // clear results

	if (have()) { // alternative (slower) path  
		dst = data; // copy
	} else { // fastest path, when stack is empty
		dst = std::move(data); // move
		data.clear();
	}

	history_pos++;
	if (history_pos >= HISTORY_LEN) {
		history_pos = 0;
	}
	}

	if (have()) newframe(); // slow path. cleanup
}

/** 
 * API implementation wrappers 
 */
ThreadData init_thread_data() {
	auto &data = impl::new_thread_data();
	//std::vector<ThreadID> v;
	//get_threads(v);
	//REQUIRE(v.size() > 0); // BAD TEST
	return data.get_wrapper();
}

void        free_thread_data(ThreadData v) {
	impl::delete_thread_data(v.data);
}

/** get ThreadData for current thread. CANNOT BE USED BEFORE INIT OR AFTER FREE! */
ThreadData get_thread_data() {
	auto &data = impl::get_thread_data();
	return data.get_wrapper();
}

/** THIS WRAPPER IS REALLY IMPORTANT NOW! */
void ThreadData::begin(const char* name) {
	const std::string *_cached = nullptr;
	std::string lookup = name;
	{
		auto ruse = impl::string_cache.use(); // lock
		auto& cache = ruse.ref;
		auto v = cache.find(lookup);
		if (v == cache.end()) {
			_cached= &(*(cache.insert(lookup).first));
		} else {
			_cached = &(*v);
		}
	}
	return data.begin(_cached);
}
void ThreadData::end() {return data.end();}
void ThreadData::step() {return data.step();}

/**
 * API for retrieving profiling results
 */

/** return vector of all threads */
void get_threads(std::vector<ThreadID>& vec) {
	vec.clear();

	auto ruse = impl::prof_history.use(); // lock
	auto& history = ruse.ref;
	vec.reserve(history.size());

	for (auto &[id, _] : history) {
		vec.push_back(id);
	}
}

/** return length of history buffers (or (max position+1) in history/summary) */
size_t history_size() {
	return HISTORY_LEN;
}

/**
 * Get statictics about all zones in one thread at specified position in time.
 * returns whole ass copy of everything.
 * Calling this every frame is absolutely costly, but
 * it allows to minisize syncronisations :)
 * 
 * Returns no value if thread with this ID does not exist.
 * pos is in range of 0 to history_size()-1;
 */
StatsStorage2 get_summary(ThreadID id, size_t pos) {
	if (pos >= HISTORY_LEN) // error (misuse)
		std::runtime_error("History position is out of range!");
	
	StatsStorage res;
	{
		auto ruse = impl::prof_history.use(); // lock
		auto& history = ruse.ref;

		auto v = history.find(id);
		if (v == history.end()) return StatsStorage2(); // invalid ThreadID
	
		// oh
		res = v->second[pos];
	}

	return res; // return a copy
}

size_t get_current_position(ThreadID id) {
	size_t res = 0;
	{
	auto ruse = impl::prof_data.use(); // lock
	auto& data = ruse.ref;

	auto v = data.find(id); // we don't want to always call a constructor
	if (v == data.end()) return 0;
	res = v->second.history_pos;
	}
	res--;
	if (res >= history_size()) res = HISTORY_LEN-1;
	return res;
}

};

};
