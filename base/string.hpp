/*
 * This file is a part of Pixelbox - Infinite 2D sandbox game
 * String utilities
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
#include <string>
#include <stdexcept>
#include "external/printf.h"

namespace pb {

	// NOT atomic!
	// You should use non-const comparison and get_hash() functions!
	// they can save hash for later use...
	// also hash is precalculated when constructed from std::string, const char* or other stuff!
	// but NOT when using any modificational operators! so make sure to regenerate hash when all
	// changes are finally done and you wish to use this string in comparing again
	class HString : public std::string {
		friend class std::hash<HString>;
		private:
		size_t hash = 0;
		size_t regen_hash() const {
			size_t seed = std::distance(begin(), end());
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

	/**
	 * Widely used string formatting function. It returns data into the std::string by value, specified
	 * at the first argument.
	 */
	template<typename ... Args>
	bool format_v(std::string& result, const std::string& format, Args ... args ) {
		result.clear();
		int size_s = snprintf_( nullptr, 0, format.c_str(), args... );
		if( size_s < 0 ) return false; // error here?

		auto size = static_cast<size_t>(size_s + 1); // Extra space for '\0'
		result.resize(size);
		auto vv = snprintf_( &result[0], size, format.c_str(), args... );
		return (vv > 0);
	}

	/**
	 * Widely used string formatting function. It returns formatted std::string.
	 * If you already have an instance of std::string with associated buffer, 
	 * consider using pb::format_v() instead!
	 */
	template<typename ... Args>
	std::string format_r(const std::string& format, Args... args ) {
		std::string result;
		if (format_v(result, format, args...)) {
			return result;
		}
		throw std::runtime_error("bad format string!");
	};

	/**
	 * Constant function to get a floating point number from string.
	 * Locale-independent
	 */
	double strtod(const char *str, char **end);

};

// here is the hash function 
template<>
struct std::hash<pb::HString> {
	std::size_t operator()(pb::HString& s) const noexcept {
		return s.get_hash();
	}
	std::size_t operator()(const pb::HString& s) const noexcept {
		return s.get_hash();
	}
};