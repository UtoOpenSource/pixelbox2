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

#pragma once
#include "base.hpp"
#include "core.hpp"

//implementation
#include <external/robin_map.h>
#include <cstddef>
#include "base/string.hpp"

namespace pb {

	// Value for associative metadata map (see below)
	// can keep direct references to values like entities and chunks.
	// But will be serialized back into UUID/Chunk position when saved
	// TODO FIXME NAN KEY STORAGING PROBLEM MUST BE FIXED!!!1
	using MetaKey = std::variant<double, HString>;

	// types of MetaValue
	enum MetaTypes {
		NIL,
		NUM,
		STR,
		ARR,
		OBJ
	};

	using MetaValue = std::variant<std::nullptr_t, double, HString, std::unique_ptr<Metadata>, weak_ptr<MetaClass>>;

	// Associative array for metadata. see below.
	using MetapropMap = tsl::robin_map<MetaKey, MetaValue>;
 
	// Chunk Associative array
	using ChunkMap = tsl::robin_map<ChunkPos, shared_ptr<Chunk>>;

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
 * We don't make this shared to prevent recursion
 */
	class Metadata : public Moveable {
		public:
		MetapropMap map;
		public:
		MetaValue& operator[](const MetaKey& key) {
			auto v = map.find(key);
			if (v == map.end()) {
				v = map.emplace_hint(v, nullptr);
			}
			return v.value();
		}
	};

	/**
	 * To make difference between Game-related Objects and just a metadata, every Game-related
	 * must inherit from MetaClass, not Metadata!
	 */
	 class MetaClass : public Metadata, public Shared<MetaClass> {

	 };

};
