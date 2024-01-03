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

/*
 * Please use compiler with C++20 concepts and general C++20 support to compile pixelbox! 
 */
#if !((defined(_MSVC_LANG) && _MSVC_LANG >= 201703L) || __cplusplus >= 201703L || PB_NO_STDVER_CHECK)
#error C++17 support is required!
#endif


#pragma once
#include <concepts>
#if !(__cpp_concepts)
#error C++20 concepts support is required!
#endif

#include <stdint.h>
#include <limits.h>

namespace pb {

	class Default {
		public:
		Default() = default;
		Default(const Default&) = delete;
		Default& operator=(const Default&) = delete;
	};

	// abstract base class... Yay :D
	class Abstract : public Default {
		public:
		virtual ~Abstract() = 0;
	};

};

