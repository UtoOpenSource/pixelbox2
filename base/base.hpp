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
#include <string>

namespace pb {

	// NOT atomic!
	// You should use non-const comparison and get_hash() functions!
	// they can save hash for later use...
	// also hash is precalculated when constructed from std::string, const char* or other stuff!
	class HString : public std::string {
		friend class std::hash<HString>;
		private:
		size_t hash = 0;
		constexpr size_t regen_hash() const {
			size_t seed = this->length();
			for(size_t x : *this) {
				x = ((x >> 16) ^ x) * 0x45d9f3b;
				x = ((x >> 16) ^ x) * 0x45d9f3b;
				x = (x >> 16) ^ x;
				seed ^= x + 0x9e3779b9 + (seed << 6) + (seed >> 2);
			}
			if (!seed) seed = 1;
			return seed;
		}
		public:
		using std::string::string;
		HString& operator=(const HString& src) {
			static_cast<std::string&>(*this) = static_cast<const std::string&>(src);
			hash = src.hash;
			return *this;
		}
		HString& operator=(HString&& src) {
			static_cast<std::string&>(*this) = static_cast<std::string&&>(src);
			hash = src.hash;
			src.hash = 0;
			return *this;
		}
		HString(const char* src) : std::string(src) {
			hash = regen_hash();
		}
		explicit HString(const std::string& src) : std::string(src) {
			hash = regen_hash();
		}
		HString(const HString& src) : std::string(src) {
			hash = src.hash;
		}
		HString(HString&& src) : std::string(src) {
			hash = src.hash;
			src.hash = 0;
		}
		inline size_t get_hash() const {
			//if (!hash) throw "Not hashed HSTRING!";
			if (!hash) return regen_hash();
			return hash;
		}
		inline size_t get_hash() {
			if (!hash) hash = regen_hash();
			return hash;
		}
		void reset_hash() {
			hash = 0;
		}
		bool operator==(HString& src) {
			if (size() == src.size() && get_hash() == src.get_hash()) {
				if (size() <= 8) return true; // let's try
				return static_cast<std::string&>(*this) == static_cast<std::string&>(src);
			}
			return false;
		}
		bool operator!=(HString& src) {
			return !(*this==src);
		}
	};

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

// here is hash function 
template<>
struct std::hash<pb::HString> {
	std::size_t operator()(pb::HString& s) const noexcept {
		return s.get_hash();
	}
	std::size_t operator()(const pb::HString& s) const noexcept {
		return s.get_hash();
	}
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