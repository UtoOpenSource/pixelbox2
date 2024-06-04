/*
 * This file is a part of Pixelbox - Infinite 2D sandbox game
 * Graphics And screen management Core
 * Copyright (C) 2024 UtoECat
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

#include <functional>
#include "imgui.h"

namespace pb {

extern class BackendRAII {
	public:
	BackendRAII() {}
	BackendRAII(const BackendRAII&) = delete;
	BackendRAII(const BackendRAII&&) {};
	virtual ~BackendRAII() = 0;
	virtual bool tick() = 0;
	virtual void clear() = 0;
	virtual void flush() = 0;
} *graphics;

namespace screen {

	void DrawAll();

	struct Register {
		public:
		Register(std::function<void(int)> f, int priority = 0);
		~Register() = default;
	};

};

};