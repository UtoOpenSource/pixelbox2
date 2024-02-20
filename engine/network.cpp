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

void ENetConnection::disconnect_now() {
	get_host()->disconnect_now(*this);
}

void ENetConnection::reset() {
	get_host()->reset_peer(*this);
}

void ENetBase::default_init() {
		if (host == nullptr) return;
		host->userdata = this;	// feelsgoodman
	}

bool ENetBase::create_server() {
	if (host) std::runtime_error("host is already exists!");
	auto addr = info.getAddress();
	host = enet_host_create(&addr, info.nconnections, info.nchannels, 0, 0);
	default_init();
	HostDefaultConfig(host);
	return host != nullptr;
}

bool ENetBase::create_client() {
	if (host) std::runtime_error("host is already exists!");
	host = enet_host_create(NULL, info.nconnections, info.nchannels, 0, 0);
	default_init();
	HostDefaultConfig(host);
	return host != nullptr;
}

void ENetBase::free_event(const ENetEvent& ev) {
	switch (ev.type) {
		case ENET_EVENT_TYPE_CONNECT:
		case ENET_EVENT_TYPE_DISCONNECT:
		case ENET_EVENT_TYPE_DISCONNECT_TIMEOUT:
		break;
		case ENET_EVENT_TYPE_RECEIVE:
			enet_packet_destroy(ev.packet);	 // hehe
			break;
		case ENET_EVENT_TYPE_NONE:
		break;
	}
}

ENetConnection* ENetBase::handle_event(const ENetEvent& ev) {
	ENetConnection* conn = static_cast<ENetConnection*>(ev.peer->data);

	switch (ev.type) {
		case ENET_EVENT_TYPE_CONNECT: {

			try {
				assert(ev.peer->data == nullptr);
				conn = new ENetConnection(ev.peer);	 // init
				ev.peer->data = conn;								 			// set
				peers.emplace(conn);
				if (!conn) throw std::runtime_error("OOM");
			} catch (std::exception& e) {
				enet_peer_reset(ev.peer);
				return nullptr;
			}

			// add event handler
			std::shared_ptr<ENetHandler> h;
			try {
				h = handler_maker();
				if (!h) throw std::runtime_error("returned nullptr!");
			} catch (std::exception& e) {
				std::cerr << "Exception in handler_maker : " << e.what() << std::endl;
				disconnect_now(*conn);
				return nullptr;
			}
			peers_count++;
			conn->set_handler(std::move(h));
			break;
		}
		case ENET_EVENT_TYPE_DISCONNECT:
		case ENET_EVENT_TYPE_DISCONNECT_TIMEOUT:
			try {
				peers_count--;
				(*conn)->net_disconnect(*conn, ev.type == ENET_EVENT_TYPE_DISCONNECT_TIMEOUT);
				raw_peer_reset(*conn);	// done
			} catch (std::exception& e) {
				std::cerr << "Exception in disconnect handler : " << e.what() << std::endl;
			}
			break;
		case ENET_EVENT_TYPE_RECEIVE:
			try {
				(*conn)->net_recieve(*conn, ev.channelID,
														std::string_view((const char*)enet_packet_get_data(ev.packet), enet_packet_get_length(ev.packet)));
			} catch (std::exception& e) {
				std::cerr << "Exception in recieve handler : " << e.what() << std::endl;
			}
			// caller must call free_event()!!
			break;
		case ENET_EVENT_TYPE_NONE:
			break;
	}
	return conn;
}

bool ENetBase::service_events(ENetEvent* ev, uint32_t timeout) {
	assert(ev != nullptr);
	assert(host != nullptr);
	return (enet_host_service(host, ev, timeout) > 0);
}

void ENetBase::destroy() {
	if (!host) return;

	foreach ([](ENetConnection& conn) {
		enet_peer_timeout(conn.peer, 2000, 1000, 5000);
		conn.disconnect();
	});

	flush();

	ENetEvent ev;
	while (peers_count && service_events(&ev, 5000)) {
		switch (ev.type) {
			case ENET_EVENT_TYPE_CONNECT:
				enet_peer_reset(ev.peer);	 // ignore and kill
				break;
			case ENET_EVENT_TYPE_DISCONNECT:
			case ENET_EVENT_TYPE_DISCONNECT_TIMEOUT:
				handle_event(ev);
				break;
			case ENET_EVENT_TYPE_RECEIVE:
			case ENET_EVENT_TYPE_NONE:
				break;
		}
		free_event(ev);
	}

	force_destroy();	// forcefully shutdown now!
};

bool ENetClient::connect(const char* ip, unsigned short port) {
	info.ip = NULL;
	info.port = port;

	if (!create_client()) return false;

	try {
		// connect
		info.ip = ip;
		const ENetAddress addr = info.getAddress();
		info.nconnections = 1;

		auto* srv = enet_host_connect(host, &addr, info.nchannels, 0);
		if (!srv) {
			throw "ENetClient : can't create peer!";
		}

		// process events
		ENetEvent event;
		while (service_events(&event, 5000) && event.type == ENET_EVENT_TYPE_CONNECT) {
			handle_event(event); // do init
			free_event(event);
			return true; // success
		}

		throw "ENetClient : can't connect to the server!";

	} catch (const char* data) {
		std::cout << "Exception : " << data << std::endl;
		force_destroy();
	}
	return false;
}

bool ENetClient::service(int timeout) {
	ENetEvent event;

	if (!get_server() && attempt_reconnect > 0 && attempt_reconnect < 10) {
		force_destroy();
		connect(info.ip, info.port);
		attempt_reconnect++;
		return true; // whatever
	}

	if (!host || !peers_count) return false;

	if (service_events(&event, timeout)) {
		handle_event(event); // do init

		if (event.type == ENET_EVENT_TYPE_DISCONNECT || event.type == ENET_EVENT_TYPE_DISCONNECT_TIMEOUT) {
			if (attempt_reconnect > 0 && attempt_reconnect < 10) {
				free_event(event);
				return true; // try again
			}
		}

		free_event(event);
	}
	return true;
}

bool ENetServer::service(int timeout) {
	ENetEvent event;

	if (!host) return false;
	if (!peers_count && !keep_working) return false;

	if (service_events(&event, timeout)) {

		if (prevent_connection && event.type == ENET_EVENT_TYPE_CONNECT) {
			enet_peer_reset(event.peer);	 // ignore and kill
			free_event(event);
			return true;
		}

		handle_event(event); // do init
		free_event(event);
	}
	return true;
}

bool ENetServer::create(const char* ip, unsigned short port) {
	info.ip = ip;
	info.port = port;

	if (!create_server()) return false;
	return true;
}


};	// namespace pb
