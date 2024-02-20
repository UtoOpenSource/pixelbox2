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

#pragma once
#include <assert.h>
#include <string.h>

#include <functional>
#include <iostream>
#include <memory>
#include <set>
#include <stdexcept>
#include <string_view>
#include <typeinfo>

#include "base/base.hpp"
#include "base/sharedobj.hpp"
#include "external/enet.h"

namespace pb {

static constexpr unsigned short DEFAULT_PORT = 4792;

struct ProtocolInfo {
	const char* ip = NULL;
	unsigned short port = DEFAULT_PORT;
	int nchannels = 5;
	int nconnections = 1;

 public:
	inline ENetAddress getAddress() {
		ENetAddress addr;
		addr.port = port;

		if (ip)
			enet_address_set_host(&addr, ip);
		else
			addr.host = ENET_HOST_ANY;
		return addr;
	}
};

class ENetBase;
class ENetServer;
class ENetClient;
class ENetConnection;

/** user-specified event handler for peer + userdata. Base class*/
class ENetHandler : public Shared<ENetHandler> {
 public:
	ENetHandler() = default;
	virtual ~ENetHandler(){};

 public:
	virtual void net_connect(ENetConnection& con) = 0;
	virtual void net_switch_out(ENetConnection& con) = 0;
	virtual void net_recieve(ENetConnection& con, uint8_t channel, std::string_view data) = 0;
	virtual void net_update(ENetConnection& con) = 0;
	virtual void net_disconnect(ENetConnection& con, bool is_timeout) = 0;
};

/** ENetPeer wrapper + event handler custom userdata */
class ENetConnection : public Static {
 protected:
	::ENetPeer* peer = nullptr;
	bool is_disconnecting = false;
	ENetConnection(::ENetPeer* ptr) : peer(ptr) { assert(peer != nullptr); };
	friend class ENetBase;

 public:
	~ENetConnection() = default;

 public:
	// userdata
	std::shared_ptr<ENetHandler> handler;
	ENetHandler* operator->() { return handler ? handler.get() : throw std::runtime_error("no handler is set for this peer"); }

 private:	 // handler caller

	bool disconnecting_state() {
		if (is_disconnecting) return true;
		is_disconnecting = true;
		return false;
	}

 public:	// methods
	operator bool() { return peer ? peer->state == ENET_PEER_STATE_CONNECTED : false; }

	ENetBase* get_host() {
		return peer ? (ENetBase*)peer->host->userdata : nullptr;
	}

	void set_handler(std::shared_ptr<ENetHandler>&& src) {
		bool run = (handler != src && src);
		if (run) {
			if (handler) handler->net_switch_out(*this);
			handler = std::move(src);
			if (handler) handler->net_connect(*this);
		}
	}

	/** @warning this object will be destructed after this and memory freed!
	 *  do not use object after calling this function.
	 *  on_disconnect callback is not called, client would not know about losing a connection.
	 */
	void reset();

	/** @warning this object will be destructed after this and memory freed!
	 * do not use object after calling this function.
	 * triggers on_disconect event
	 */
	void disconnect_now();

	/** @warning do not call this inside net_disconnect!
	 */
	bool disconnect() {
		assert(peer != nullptr);

		// hehe
		if (disconnecting_state()) {
			return false;
		}

		enet_peer_disconnect(peer, 0);
		return true;
	}

	/** @warning do not call this inside net_disconnect!
	 */
	bool disconnect_later() {
		assert(peer != nullptr);

		// hehe
		if (disconnecting_state()) {
			return false;
		}

		enet_peer_disconnect_later(peer, 0);
		return true;
	}

	bool send(uint8_t channel, ENetPacket* data) {
		assert(peer != nullptr);
		return enet_peer_send(peer, channel, data) == 0;
	}

	bool send(uint8_t channel, std::string_view data) {
		assert(peer != nullptr);
		ENetPacket* p = enet_packet_create(data.data(), data.size(), 0);
		return p ? send(channel, p) : false;
	}

	bool operator<(const ENetConnection& with) { return peer < with.peer; }
};

using ENetHandlerMaker = std::function<std::shared_ptr<ENetHandler>()>;

class ENetBase : public Static {
	friend class ENetConnection;
 protected:
	ENetHost* host = nullptr;
	ProtocolInfo info;

	// don't ask
	std::set<ENetConnection*> peers;
	size_t peers_count = 0; // count of peers

	/** initial handler */
	ENetHandlerMaker handler_maker;
	void default_init();

	/**
	 * Gets event from the queue. You should call handle_event() 
	 * and must call free_event(). Return false if there is no events left
	 *
	 * @param timeout in milliseconds
	 */
	bool service_events(ENetEvent* ev, uint32_t timeout = 0);

	/// default handlier. Returns associated Connection (or nullptr on error!)
	ENetConnection* handle_event(const ENetEvent& ev);

	// deinitialize event
	void free_event(const ENetEvent& ev);

	void raw_peer_reset(ENetConnection& conn) {
		if (conn.peer) conn.peer->data = nullptr;
		conn.peer = nullptr;
		peers.erase(&conn);
		delete &conn;
	}

 public:
	ENetBase() = delete;
	ENetBase(const ENetBase&) = delete;
	ENetBase(ENetHandlerMaker f) : handler_maker(f) {
		if (!handler_maker) std::runtime_error("Handler maker function is required!");
	}

	operator bool() { return host != nullptr; }
	void set_address(ProtocolInfo i) { info = i; }

	/** calls function at the first argument on all the peers connected to this host.
	  * it's safe to disconnect/reset peer, passed in callback :) but not any another!
		*/
	void foreach (std::function<void(ENetConnection&)> cb) {
		if (host != nullptr) return;

		auto it = peers.begin();
		for (; it != peers.end();) {
			auto *conn = *it;
			it++;
			cb(*conn);
		}
	}

	/**
	 * iterate over all peers, but runs callback only on connections, that have
	 * specific handler associated, including subtypes.
	 * 
	 * call of foreach() is equal to call foreach_handler<ENetHandler>(), except for extra
	 * callack argument - handler itself :p
	 *
	 * it's safe to disconnect/reset peer, passed in callback :) but not any another!
	 */
	template <typename HT>
	void foreach_handler(std::function<void(ENetConnection&, std::shared_ptr<HT>)> cb) {
		if (host != nullptr) return;

		auto it = peers.begin();
		for (; it != peers.end();) {
			auto *conn = *it;
			it++;
			auto handler = std::dynamic_pointer_cast<HT>(conn->handler);
			if (handler) cb(*conn, handler);
		}
	}

	/** you cannot use this connection after this */
	void reset_peer(ENetConnection& conn) {
		enet_peer_reset(conn.peer);
		raw_peer_reset(conn);
	}

	/** you cannot use this connection after this */
	void disconnect_now(ENetConnection& conn) {
		enet_peer_disconnect_now(conn.peer, 0);
		conn->net_disconnect(conn, 0);
		raw_peer_reset(conn);
	}

	void force_destroy() {
		// cleanup all userdata
		foreach ([](ENetConnection& conn) { conn.reset(); });
		assert(peers.size() == 0);

		if (host) enet_host_destroy(host);
		host = nullptr;
	}

	/** flushes all packages to be finally sended! No need to do if you regulary call a service()! */
	void flush() {
		assert(host != nullptr);
		enet_host_flush(host);
	}

	virtual ~ENetBase() { force_destroy(); }

	void destroy();

	/** Creates server host used address and protocol info specified in set_address. Returns false on error */
	bool create_server();

	/** Creates client host used address and protocol info specified in set_address Returns false on error */
	bool create_client();
};

class ENetClient : public ENetBase {
 public:
	/** @property if > 0 and < 10, client will try to reconnect to the server */
	int attempt_reconnect = 0;

	ENetClient(ENetHandlerMaker f) : ENetBase(f) {};

	ENetConnection* get_server() {
		return peers.begin() == peers.end() ? nullptr : *(peers.begin());
	}

	~ENetClient() {
		// all other stuff is done in base
	}

	/** create AND connect a client to the specified server! */
	bool connect(const char* ip, unsigned short port = DEFAULT_PORT);

	/** function for a user :) */
	void disconnect() {
		destroy();	// destroy connection, all user data, etc.
	}

	/* same as running() + extra check */
	bool is_connected() { return get_server() != nullptr; }

	/** @warning channel starts FROM ZERO!!!! */
	bool send(uint8_t channel, ENetPacket* data) {
		auto* server = get_server();
		if (!server || !host) return false;
		return server->send(channel, data);
	}

	/** @warning channel starts FROM ZERO!!!! */
	bool send(uint8_t channel, const std::string_view data) {
		auto* server = get_server();
		if (!server || !host) return false;
		ENetPacket* p = enet_packet_create(data.data(), data.size(), 0);
		if (!p) return false;
		return send(channel, p);
	}

	/** services client internally. Returns true if service() should be called again. False if you should disconnect()! */
	bool service(int timeout = 0);

};


class ENetServer : public ENetBase {
 public:
	/** @property server will continue servicing (return true from service()) even when no active connections left
	 * you should not set this property before create()! because server will never listen to connections in that case
	 */
	bool keep_working = true;

	/** @property server will not accept any connections anymore, and serve only already connected clients 
	 */
	bool prevent_connection = false;

 public:
	ENetServer(ENetHandlerMaker f) : ENetBase(f) {};

	/** services server internally. Returns true if service() should be called again. False if you should shutdown()! */
	bool service(int timeout = 0);

	bool create(const char* ip, unsigned short port = DEFAULT_PORT);

	void shutdown() {
		ENetBase::destroy();	// destroy connection, all user data, etc, and fire all callbacks
	}

	void destroy() {
		ENetBase::force_destroy();	// immediate destroy!
	}

	~ENetServer() {} // all work is done in the base class!

	void broadcast(uint8_t channel, ENetPacket* data) {
		foreach([channel, data](ENetConnection& conn) {
			conn.send(channel, data);
		});
	}

	void broadcast(uint8_t channel, const std::string_view v) {
		if (!host) return;
		ENetPacket* p = enet_packet_create(v.data(), v.size(), 0);
		if (!p) return;
		broadcast(channel, p);
	}

};

};	// namespace pb

#if 0
/**
 * @description ENet Wrapper
 */
namespace pb_old {


};	// namespace pb_old
#endif
