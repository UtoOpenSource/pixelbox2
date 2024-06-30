/*
 * This file is a part of Pixelbox - Infinite 2D sandbox game
 * THE WORLD
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

#include "base.hpp"
#include <stdint.h>
#include <new>
#include "hashmap.hpp"

 namespace pb {

	enum {
		PIX_NUL = 0,
		PIX_AIR = 1,
		PIX_ALL = 256 // MAX + 1
	};

	typedef unsigned char u8;
	typedef uint64_t u64;
	typedef unsigned int u32;
	typedef unsigned short u16;
	static_assert(u8(PIX_ALL) == 0 && u8(PIX_ALL-1) == PIX_ALL-1, "unisgned char is weird on your platform and not supported!");

	struct alignas(u64) Pixels {
		static constexpr int CHUNK_WIDTH = 16;
		static constexpr int CHUNK_SIZE  = CHUNK_WIDTH*CHUNK_WIDTH;
		public:
		alignas(u64) u8 data[CHUNK_SIZE]; // for optimisations
		using BIG_TYPE = u64;
		static constexpr int BIG_SIZE = 8;
		public: 
		inline void zero() {
			for(u32 i = 0; i < CHUNK_SIZE; i++) data[i] = 0;
		}
		void combine_from(const Pixels& src) {
			for (u32 i = 0; i < CHUNK_SIZE; i++) {
				if (src.data[i]) data[i] = src.data[i];
			}
		}
	};
	static_assert(sizeof(Pixels) == Pixels::CHUNK_SIZE, "weird");
	static_assert(alignof(Pixels) == alignof(u64));

	union ChunkCoords {
		u16 part[2];
		u32 combo;
	};
	static_assert(sizeof(ChunkCoords) == sizeof(uint32_t));

	static constexpr short GC_MARK = 50;
	struct Chunk {
		public:
		ChunkCoords pos;
		short       gc_info = GC_MARK;
		bool        is_ready : 1 = false; // ready or invalid still
		bool        in_free_list : 1 = false; // must be deleted frpom free list if will be recruited
		bool        is_changed : 1 = false; // unchanged since lload/gen or last global save chunks shall not be saved again
		Pixels      zone_a, zone_b; // threading zones
	};
	// TODO :  add BBOX i8 and etc.

	struct WorldStorage {
		public:
		bool is_zone_b = false; // 0 => zone_a, 1=>zone_b
		pb::HashMap<ChunkCoords, Chunk*, hash_obj<ChunkCoords>> chunk_map; // use murmurhash for pos

		// secondary
		// load queue may still be here,but save_queue will be moved out into other master container
		pb::HashMap<ChunkCoords, Chunk*, hash_obj<ChunkCoords>> load_queue; // chunks to be loaded, already present in chunk_map with laoded=False
		pb::HashMap<ChunkCoords, Chunk*, hash_obj<ChunkCoords>> save_queue; // save queue for chunks
		public:

		/// get chunk only if it actually exists, else nullptr
		inline Chunk* getPresentChunk(ChunkCoords pos) const {
			auto v = chunk_map.find(pos);
			if (v != chunk_map.end()) return v->second;
			return nullptr; // no
		}

		/// gets chunk anyway. If not exist, adds new chunk to map and load_queue, to be processed by other systems later 
		inline Chunk* getChunk(ChunkCoords pos) noexcept {
			auto v = chunk_map.find(pos);
			if (v != chunk_map.end()) return v->second;
			// stuff is going on
			auto* o = new(std::nothrow) Chunk{.pos=pos};
			if (!o) return nullptr; // alloc error
			o->is_ready = false;
			chunk_map.insert(pos, o);
			load_queue.insert(pos, o);
			return o;
		}

		/// collects chunks from map into save queue. If chunk was not loaded yet, remioves it from load queue
		void collectChunks(int amount = 1) {
			auto i = chunk_map.begin();
			while (i != chunk_map.end()) {
				assert(i->second != nullptr);
				if (i->second->gc_info > 0) i->second->gc_info -= amount; // mark
				if (i->second->gc_info <= 0) { // collected
					if (!i->second->is_ready) load_queue.erase(i->first); // DELETE FROM LOAD QUEUE
					save_queue.insert(i->first, i->second); // save later if conditions met
					i = chunk_map.erase(i); // remove :)
				} else { // still alive
					i++;
				}
			}
		}

	};

 }