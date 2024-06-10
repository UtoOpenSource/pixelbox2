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

#ifdef __GNUC__
# if ((__GNUC__ == 4 && __GNUC_MINOR__>= 4) || __GNUC__ > 4)
#  define ATTR_PRINTF(one_based_format_index, first_arg) \
__attribute__((format(gnu_printf, (one_based_format_index), (first_arg))))
# else
# define ATTR_PRINTF(one_based_format_index, first_arg) \
__attribute__((format(printf, (one_based_format_index), (first_arg))))
# endif
# define ATTR_VPRINTF(one_based_format_index) ATTR_PRINTF((one_based_format_index), 0)
#else
# define ATTR_PRINTF(one_based_format_index, first_arg)
# define ATTR_VPRINTF(one_based_format_index)
#endif

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

	extern void log_func(const char* file, int line, int level, const char* fmt, ...) noexcept ATTR_PRINTF(4, 5);
	[[noreturn]] extern void terminate();
};

#define LOG_ANY(level, fmt, ...) pb::log_func(__FILE__, __LINE__, level, fmt ,##__VA_ARGS__)
/// this one will terminate thread/prgram
#define LOG_FATAL(...) do{LOG_ANY(0, __VA_ARGS__); pb::terminate();} while(0)
#define LOG_ERROR(...) LOG_ANY(1, __VA_ARGS__)
#define LOG_WARN(...) LOG_ANY(2, __VA_ARGS__)
#define LOG_INFO(...) LOG_ANY(3, __VA_ARGS__)
#define LOG_DEBUG(...) LOG_ANY(4, __VA_ARGS__)

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
