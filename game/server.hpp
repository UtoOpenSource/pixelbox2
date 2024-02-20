/*
 * This file is a part of Pixelbox - Infinite 2D sandbox game
 * Server setup logic
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

#include <iostream>
#include <memory>
#include "engine/network.hpp"
#include "base.hpp"
#include "external/raiisqlite.hpp"

namespace pb {

	class GamerServerBase {
		protected:
		std::unique_ptr<ENetServer> server;
    sqlite::Database database;
		public:
		GamerServerBase(const char* db_file, int port, int max_cli = 1) {
			// create server
			server = std::make_unique<ENetServer>(ProtocolInfo(NULL, port, 5, max_cli));
			if (!db_file) {
				std::cerr << "No file specified - in memory database is used for testing" << std::endl;
				db_file = ":memory:";
			}
			database.open(db_file);
			db::world_settings(database); // configure PRAGMA's for world storade db
		}
	};

};
