/*
 * This file is a part of Pixelbox - Infinite 2D sandbox game
 * Chunk implementation
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
#include "core.hpp"
#include "meta.hpp"

//implementation
#include <external/robin_map.h>

namespace pb {

	class Chunk : public Metadata {
		public:
		ChunkPos pos;
		struct Pixels data, diff; // readonly + written changes
	};

};
