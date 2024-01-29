/*
 * This file is a part of Pixelbox - Infinite 2D sandbox game
 * Enet wrapper!
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

#include <base.hpp>
#include <functional>
#include <memory>
#include <set>
#include <stdexcept>
#include <string>

#include "enet.h"

/**
 * @description ENet Wrapper
 */
namespace pb {

	static void HostDefaultConfig(ENetHost* host) {
		// TODO Adjust to better values!
		host->maximumPacketSize = 1024 * 32;
		host->maximumWaitingData = 1024 * 128;
	}

/*
void ENetBase::on_event_recv(ENetEvent& ev) {
	auto conn = ENetConnection(ev.peer);
	switch (ev.type) {
		case ENET_EVENT_TYPE_CONNECT:
			enet_peer_set_data(ev.peer, new ConnectionData());
			h_connect.notify_all(*this, conn);
			break;
		case ENET_EVENT_TYPE_DISCONNECT:
		case ENET_EVENT_TYPE_DISCONNECT_TIMEOUT:
			try {
				h_disconnect.notify_all(*this, conn);
			} catch (std::exception& e) {
				std::cerr << "Exception in disconnect handler : " << e.what() << std::endl;
			} catch (...) {
				std::cerr << "Unknown exception in disconnect handler" << std::endl;
			}
			{
				auto v = (ConnectionData*)enet_peer_get_data(ev.peer);
				if (v) delete v;
				enet_peer_set_data(ev.peer, nullptr);
			}
			break;
		case ENET_EVENT_TYPE_RECEIVE:
			try {
				h_recieve.notify_all(*this, conn, ev.channelID,
														 std::string_view((const char*)enet_packet_get_data(ev.packet), enet_packet_get_length(ev.packet)));
			} catch (std::exception& e) {
				std::cerr << "Exception in message handler : " << e.what() << std::endl;
			}
			enet_packet_destroy(ev.packet);	 // hehe
			break;
		case ENET_EVENT_TYPE_NONE:
			break;
	}
}

void ENetBase::destroy() {
	if (!host) return;

	foreach ([](ENetPeer* peer) {
		enet_peer_timeout(peer, 2000, 1000, 5000);
		enet_peer_disconnect(peer, -1); // shutdown
	});

	flush();
	int attempts = host->peerCount * 8;	 // process as much as *8 events
	int dummies = host->peerCount;

	ENetEvent ev;
	while (dummies > 0 && attempts > 0 && enet_host_service(host, &ev, 5000)) {
		switch (ev.type) {
			case ENET_EVENT_TYPE_CONNECT:
				enet_peer_reset(ev.peer);	 // ignore and kill
				break;
			case ENET_EVENT_TYPE_DISCONNECT:
			case ENET_EVENT_TYPE_DISCONNECT_TIMEOUT:
				dummies--;
				on_event_recv(ev);	// default behaviour
				break;
			case ENET_EVENT_TYPE_RECEIVE:
				enet_packet_destroy(ev.packet);	 // we don't care
				break;
			case ENET_EVENT_TYPE_NONE:
				break;
		}
	}

	force_destroy();	// shutdown now!
};

bool ENetClient::connect(const char* ip, unsigned short port) {
	info.ip = NULL;
	info.port = port;
	
	create_client();
	server = nullptr;

	if (!host) {
		std::cerr << "ENetClient: can't create client host! (weird)" << std::endl;
		return false;	 // :(
	}

	// connect
	info.ip = ip;
	const ENetAddress addr = info.getAddress();

	server = enet_host_connect(host, &addr, info.nchannels, 0);
	if (!server) {
		std::cerr << "ENetClient : can't create peer!" << std::endl;
		goto err;
	}

	// process events
	ENetEvent event;
	while (enet_host_service(host, &event, 5000) > 0 && event.type == ENET_EVENT_TYPE_CONNECT) {
		on_event_recv(event);	 // do init
		return true;
	}

	std::cerr << "ENetClient : can't connect to the server!" << std::endl;
err:
	force_destroy();
	return false;
} */

};	// namespace pb
