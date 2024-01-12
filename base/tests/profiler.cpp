/*
 * This file is a part of Pixelbox - Infinite 2D sandbox game
 * Profiler unit testing
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
#include "base/profiler.hpp"
#include "base/doctest.h"
#include <algorithm>
#include <thread>
#include <chrono>
#include <iostream>
#include <signal.h>

#define BRK() //raise(SIGTRAP); // At the location of the BP.

using std::this_thread::sleep_for;

static void gen_unique(std::vector<int>& vec, int count) {
	for (int i = 0; i < count; i++) {
		int v = std::rand() % (count*2);
		while (std::find(vec.begin(), vec.end(), v) != vec.end()) {
			v += 1;
			if (v >= count) v = 0;
		}
		vec.push_back(v); vec.push_back(v); vec.push_back(v);
	}
}

static void dump_profiler() {
	for (auto& id : pb::prof::get_threads()) {
			std::cout << "Thread ID : " << id << std::endl;
			for (size_t i = 0; i < pb::prof::history_size(); i++) {
				bool fired = false;
				for (auto [k, v] : pb::prof::get_summary(id, i)) {
					fired = true;
					std::cout << "> " << k << " : { calls=" << v.ncalls << ", owntime=" << v.owntime << ", sumtime=" << v.sumtime << "}" << std::endl;
				}
				if (fired) std::cout << "== POS " << i <<" END ===" << std::endl;
			}
			std::cout << "===================" << std::endl;
		}
}

static void thread_work(int ti) {
	auto prof = pb::prof::get_thread_data();

	sleep_for(std::chrono::milliseconds(100));
	std::vector<int> data;
	//auto _ = prof.make_zone("tick_zone");
	PROFILING_SCOPE("TICK", prof)

				{
					auto _ = prof.make_zone("init zone");
					int max = 100*ti;
					gen_unique(data, max);
				}

				{
					auto _ = prof.make_zone("sorting and halfing zone");
					std::sort(data.begin(), data.end());
					data.erase(std::remove_if(data.begin(), data.end(), [](int i) {return i % 2 == 1;}));
				}

				{
					auto _ =prof.make_zone("shrinking and result zone");
					volatile int res = 0;
					data.shrink_to_fit();
					for (int i : data) {
						res = i;
						//sleep_for(std::chrono::milliseconds(1));
					}
				}
		BRK();
		prof.step();
}

#include <iostream>

TEST_CASE("profiler_test") {

	SUBCASE("sINGLE THREAD") {
		auto prof = pb::prof::get_thread_data();
		auto _ = prof.make_zone("sus");
		for (int i = 1; i < 5; i++) {
			thread_work(1);
		}

		dump_profiler();
		
	}

	SUBCASE("parallel threads") {
		std::vector<std::thread> workers;
		for (int ti = 1; ti < 10; ti++) {
			workers.push_back(std::thread([ti]() {
				auto _td = pb::prof::make_thread_data();
				
				for (int i = 1; i < 5; i++) {
					thread_work(ti);
				}

			}));
		}

		for (auto& t : workers) {
			t.join();
		}

		dump_profiler();
	}


}