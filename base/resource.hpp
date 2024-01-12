/*
 * This file is a part of Pixelbox - Infinite 2D sandbox game
 * Resource, shared between threads
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
#include <mutex>

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