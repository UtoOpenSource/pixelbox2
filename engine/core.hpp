/*
 * This file is a part of Pixelbox - Infinite 2D sandbox game
 * Core object game systems build on.
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
#include <stdint.h>

#include <base/base.hpp>
#include <base/sharedobj.hpp>

#include <variant>
#include <algorithm>

namespace pb {

	// predefinitions
	// base
	class Metadata;
	// sertver-side only
	class World;
	class Player;	
	class Chunk;
	// client-side and in between on server
	class ChunkCache;
	// client only
	class WorldCache;
	//class Entity;

	// core definitions
	static constexpr int16_t CHUNK_WIDTH = 16;
	static constexpr int16_t CHUNK_BYTES = CHUNK_WIDTH*CHUNK_WIDTH;

	/** Minimal Pixel container. For rendering and clients.
	Also used as storage for per-pixel metadata.
	*/
	struct Pixels {
		uint8_t data[CHUNK_BYTES];	// pixel type
		public:
		Pixels(const Pixels&) = default;
		Pixels(Pixels&&) = default;
		Pixels& operator=(const Pixels&) = default;
		Pixels& operator=(Pixels&&) = default;
		const uint8_t& operator[](int i) const {return data[i];}
		uint8_t& operator[](int i) {return data[i];}
		constexpr size_t size() {return CHUNK_BYTES;}
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
		ChunkPos(const ChunkPos&) = default;
		ChunkPos(ChunkPos&&) = default;
		ChunkPos& operator=(const ChunkPos&) = default;
		ChunkPos& operator=(ChunkPos&&) = default;
		ChunkPos(int16_t x, int16_t y) {
			v.axis[0] = x;
			v.axis[1] = y;
		}
		ChunkPos(uint32_t p) {v.pack = p;}
		bool operator==(const ChunkPos& src) {return v.pack == src.v.pack;}
		bool operator!=(const ChunkPos& src) {return !(*this == src);}
	};

	// update bounds inside chunk
	struct Bounds {
		int16_t x, y, x2, y2;
		public:
		Bounds(int16_t x, int16_t y, int16_t x2, int16_t y2)
				: x(x), y(y), x2(x2), y2(y2) {}
		Bounds(const Bounds&) = default;
		Bounds(Bounds&&) = default;
		Bounds& operator=(const Bounds&) = default;
		Bounds& operator=(Bounds&&) = default;
		public:
		static constexpr int16_t max_val = CHUNK_WIDTH-1;
		/** makes invalid order of points => no bounds set */
		void reset() {x = max_val; y = max_val; x2 = 0; y2 = 0;}
		Bounds() {reset();}
		/** check are bounds exists => valod order of points*/
		operator bool() {
			return x >= x2 && y >= y2;
		}
		void check_arg(int16_t v) {
			if (v < 0 || v > max_val) throw "bad bounds value to be set";
		}
		/** extends or sets bounds to include another one point in bounds of valid ranges */
		void include(int16_t ax, int16_t ay) {
			check_arg(ax); check_arg(ay);
			x = std::min(x, ax);
			y = std::min(y, ay);
			x2 = std::max(x2, ax);
			y2 = std::max(ay, y2);
		}
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
