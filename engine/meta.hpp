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
#include "core.hpp"

//implementation
#include <external/robin_map.h>

namespace pb {

	// Value for associative metadata map (see below)
	// can keep direct references to values like entities and chunks.
	// But will be serialized back into UUID/Chunk position when saved
	using MetaKey = std::variant<HString, double>;
	using MetaValue = std::variant<double, HString, shared_ptr<Shared>>;

	// Associative array for metadata. see below.
	using MetapropMap = tsl::robin_map<MetaKey, MetaValue>;

	// Chunk Associative array
	using ChunkRobinMap = tsl::robin_map<ChunkPos, struct Chunk*>;

/*
 * Every Object that relates to the game itself : Chunk, Entyty,
 * Player, etc. Always has special storage for any amount of key=value
 * paired data - METADATA.
 *
 * Altrough name suggests taht this is a minor thing, sometimes 
 * metadata plays a key role in some systems. It allows code to be
 * dynamic and less depended on exact object structures!
 *
 * All mods are primary use ONLY metadata. 
 */

	class Metadata : public Shared {
		public:
		MetapropMap map;

	};

	/* TODO: implement later
	class Entity : public Metadata {
		class Entity* next; // next entity in chunk
		UUID id;
		int16_t x, y; // shound be normalized to chunk
	};
	*/

	class Chunk : public Metadata {
		ChunkPos pos;
		struct Pixels data, diff; // readonly + written changes
	};

};
