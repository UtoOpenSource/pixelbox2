/*
 * This file is a part of Pixelbox - Infinite 2D sandbox game
 * Copyright (C) 2023-2024 UtoECat
 *
 * This program is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see
 * <https://www.gnu.org/licenses/>
 */

#pragma once
#include <atomic>
#include <mutex>
#include <thread>

namespace pb {

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

};


