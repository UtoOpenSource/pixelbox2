/*
 * This file is a part of Pixelbox - Infinite 2D sandbox game
 * Base and wieldly used definitions
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


/*
 * Please use compiler with C++20 concepts and general C++20 support to compile pixelbox! 
 */
#pragma once
#if !((defined(_MSVC_LANG) && _MSVC_LANG >= 201703L) || __cplusplus >= 201703L || PB_NO_STDVER_CHECK)
#error C++17 support is required!
#endif

#include <stdint.h>
#include <limits.h>

namespace pb {

	/** Useful OOP Base classes */

	/** Static class : static position in memory after creation. Non-copyable, Non-movable */
	class Static {
		protected:
		Static() = default;
		public:
		Static& operator=(const Static&) = delete;
		Static& operator=(Static&&) = delete;
		Static(const Static&) = delete;
		Static(Static&&) = delete;
	};

	/** Copyable & movable class */
	class Copyable {
		protected:
		Copyable() = default;
		public:
		Copyable(Copyable&&) = default;
		Copyable& operator=(const Copyable&) = default;
		Copyable& operator=(Copyable&&) = default;
		Copyable(const Copyable&) = default;
	};

	class Moveable {
	 protected:
		Moveable() = default;
	 public:
		Moveable(const Moveable&) = delete;
		Moveable(Moveable&&) = default;
		Moveable& operator=(const Moveable&) = delete;
		Moveable& operator=(Moveable&&) = default;
	};

	/** @deprecated */

	/** Abstract base class */
	class Abstract {
		public :
		virtual ~Abstract() = 0;
	};

	using Virtual = Abstract;

};

/*
 * MACROS MAGIC AT THE END
 */
#define _CONCAT_IMPL(s1, s2) s1##s2
#define CONCAT(s1, s2) _CONCAT_IMPL(s1, s2)
#ifdef __COUNTER__ // not standard and may be missing for some compilers
#define ANONYMOUS(x) CONCAT(x, __COUNTER__)
#else // __COUNTER__
#define ANONYMOUS(x) CONCAT(x, __LINE__)
#endif // __COUNTER__
