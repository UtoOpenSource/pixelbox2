/*
 * This file is a part of Pixelbox - Infinite 2D sandbox game
 * Native Library API, used trough FFI
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
#include <stdint.h>

#include <base/base.hpp>
#include <base/sharedobj.hpp>

// implementation
//#include "libs/robin_map.h"
#include <string>
#include <variant>
namespace pb {

	// predefinitions
	class World;
	class Metadata;
	class Chunk;
	//class Entity;

	// core definitions
	static constexpr int16_t CHUNK_WIDTH = 16;
	static constexpr int16_t CHUNK_BYTES = CHUNK_WIDTH*CHUNK_WIDTH;

	struct Pixels {
		uint8_t data[CHUNK_BYTES]; // pixel type
		uint8_t meta[CHUNK_BYTES]; // optional meta info (not rendered?)
	};

	// position of a chunk in the world
	struct ChunkPos {
		public:
		union {
			int16_t axis[2];
			uint32_t pack;
		} v;
		public:
		ChunkPos() {v.pack = 0;}
		ChunkPos(int16_t x, int16_t y) {v.axis[0] = x; v.axis[1] = y;}
		ChunkPos(uint32_t p) {v.pack = p;}
		bool operator==(const ChunkPos& src) {return v.pack == src.v.pack;}
		bool operator!=(const ChunkPos& src) {return !(*this == src);}
	};

	// update bounds inside chunk
	struct Bounds {
		int16_t x, y, w, h;
	};

	// UUID. Used all over the place :p
	class UUID {
		uint8_t v[16];
		public:
		inline uint8_t& operator[](size_t i) {return v[i];}
		inline uint8_t operator[](size_t i) const {return v[i];}
		inline size_t size() const {return 16;}
		public:
		void generate(); // generates new UUID
		void zero(); // sets UUID to NULL
		public:
		UUID(std::nullptr_t) {zero();}
		UUID() {generate();} 
		UUID(const UUID& src) = default;
		~UUID() = default;
		inline bool operator==(const UUID& src) const {
			for (size_t i = 0; i < size(); i++)
				if (v[i] != src[i]) return false;
			return true;
		}
		inline bool operator!=(const UUID& src) const {return !(*this == src);}
		operator bool() const {return *this == UUID(nullptr);} // check is NULL uuid
	};

};
