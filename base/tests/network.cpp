/*
 * This file is a part of Pixelbox - Infinite 2D sandbox game
 * Network unit testing
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
#include "engine/network.hpp"

#include <algorithm>
#include <chrono>
#include <exception>
#include <iostream>
#include <memory>
#include <string_view>
#include <thread>

#include "base/base.hpp"
#include "base/doctest.h"
#include "external/enet.h"

class NetTestData : public pb::Abstract {
 public:
	int data;

 public:
	NetTestData() {}
	NetTestData(int v) : data(v) {}
	virtual ~NetTestData() {}
};

static void print_address(pb::ProtocolInfo info) {
	char buff[128];
	auto v = info.getAddress();
	enet_address_get_host_ip_new(&v, buff, 127);
	std::cerr << std::string(buff) << std::endl;
}

TEST_CASE("network_test") {
	/*SUBCASE("check address") {
		pb::ProtocolInfo info;
		char buff[128];
		auto v = info.getAddress();
		enet_address_get_host_ip_new(&v, buff, 127);
		std::cerr << std::string(buff) << std::endl;

		info.ip = "localhost";
		info.port = 90;
		v = info.getAddress();
		enet_address_get_host_ip_new(&v, buff, 127);
		std::cerr << std::string(buff) << std::endl;

		info.ip = "192.168.0.1";
		info.port = 90;
		v = info.getAddress();
		enet_address_get_host_ip_new(&v, buff, 127);
		std::cerr << std::string(buff) << std::endl;
	}*/

	pb::ProtocolInfo info;
	info.port = 4783;
	info.ip = "127.0.0.1";
	REQUIRE(info.nchannels > 3);
	info.nconnections = 1;

	SUBCASE("single thread") {
		pb::ENetServer server(info);
		REQUIRE(server.create() == true);

		server.service();

		server.destroy();
	}

	SUBCASE("parallel threads") {
		std::vector<std::thread> workers;	 // clients
		for (int ti = 1; ti < 4; ti++) {
			workers.push_back(std::thread([info, ti]() {
				std::this_thread::sleep_for(std::chrono::seconds(1));
				pb::ENetClient client(info);

				client.h_connect.subscribe([ti](pb::ENetBase&, pb::ENetConnection& con) { con.data() = std::make_unique<NetTestData>(ti * 100); });

				client.h_disconnect.subscribe([](pb::ENetBase&, pb::ENetConnection& con) {
					auto data = std::dynamic_pointer_cast<NetTestData>(con.data());
					std::cout << pb::format_r("client disconnected with stats (%i)!", data->data) << std::endl;
				});

				client.h_recieve.subscribe([](pb::ENetBase& virt, pb::ENetConnection& con, int ch, std::string_view v) {
					auto& real = dynamic_cast<pb::ENetClient&>(virt);
					auto data = std::dynamic_pointer_cast<NetTestData>(con.data());
					if (data->data % 2 == 1) {
						real.send(1, pb::format_r("amogus %i", data->data++));
					} else if (data->data == 3) {
						real.send(1, pb::format_r("omegasus %i (%s)", data->data++, v.data()));
					} else {
						real.send(1, pb::format_r("stopping myself (%s)", v.data()));
						real.defer_destroy = true;	// ok
					}

					if (data->data % 2 == 0)
					std::cout << pb::format_r("Client (%i) server echo is : ", data->data) << v << std::endl;
				});

				REQUIRE(client.connect(info.ip, info.port) == true);

				REQUIRE(client.running() == true);

				while (client.is_connected()) {
					client.service(nullptr, 1000);
					std::cout << "CLIENT: tick" << std::endl;
				}

				REQUIRE(client.running() == false);

				client.disconnect();
			}));
		}

		pb::ProtocolInfo info2 = info;
		info2.ip = NULL;
		info2.nconnections = 10;
		REQUIRE(info2.nchannels > 3);

		pb::ENetServer server(info2);
		server.shutdown_on_leaving = true;
		int i = 0;
		int j = 0;

		server.h_connect.subscribe([&i, &j](pb::ENetBase& virt, pb::ENetConnection& con) {
			con.data() = std::make_unique<NetTestData>(100 * i++); 
			std::cout << pb::format_r("Server : client (%i) connected!", 100*i) << std::endl;

			auto& real = dynamic_cast<pb::ENetServer&>(virt);
			if ((++j) % 4 == 3) {
				real.broadcast(1, pb::format_r("random echo (%s)", "AMOGUS FOR EVERYONE1111!!"));
				real.defer_destroy = true;	// ok
			}
		});

		server.h_disconnect.subscribe([](pb::ENetBase&, pb::ENetConnection& con) {
			auto data = std::dynamic_pointer_cast<NetTestData>(con.data());
			std::cout << pb::format_r("Server : client (%i) disconnected!", data->data) << std::endl;
		});

		server.h_recieve.subscribe([&j](pb::ENetBase& virt, pb::ENetConnection& con, int ch, std::string_view v) {
			auto& real = dynamic_cast<pb::ENetServer&>(virt);
			auto data = std::dynamic_pointer_cast<NetTestData>(con.data());
			std::cout << pb::format_r("Server : from(%i) : ", data->data) << v << std::endl;

			if ((++j) % 3 == 1) {
				real.broadcast(1, pb::format_r("random echo (%s)", v.data()));
				real.defer_destroy = true;	// ok
			}

		});

		REQUIRE(server.create() == true);
		server.shutdown_on_leaving = true;
		std::cout << pb::format_r("Server : init success!") << std::endl;

		REQUIRE(server.running() == true);
		while (server.running()) {
			ENetEvent ev;
			server.service(&ev, 1000);
			std::cout << pb::format_r("SERVER: tick (%i)", ev.type) << std::endl;
		}

		std::cout << "pass the server loop" << std::endl;

		server.destroy();
		REQUIRE(server.running() == false);

		for (auto& t : workers) {
			t.join();
		}
	}
}