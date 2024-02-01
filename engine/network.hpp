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

	/** calls function at the first argument on all the peers connected to this host.*/
	void foreach (std::function<void(ENetConnection&)> cb) {
		assert(host != nullptr);

		for (ENetConnection* conn : peers) {
			cb(*conn);
		}
	}

	/**
	 * iterate over all peers, but runs callback only on connections, that have
	 * specific handler associated, including subtypes.
	 * 
	 * call of foreach() is equal to call foreach_handler<ENetHandler>(), except for extra
	 * callack argument - handler itself :p
	 */
	template <typename HT>
	void foreach_handler(std::function<void(ENetConnection&, std::shared_ptr<HT>)> cb) {
		assert(host != nullptr);

		for (ENetConnection* conn : peers) {
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

};

class ENetServer : public ENetBase {

};

};	// namespace pb

#if 0
/**
 * @description ENet Wrapper
 */
namespace pb_old {

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

static void HostDefaultConfig(ENetHost* host) {
	// TODO Adjust to better values!
	host->maximumPacketSize = 1024 * 32;
	host->maximumWaitingData = 1024 * 128;
}

* represents any data *
using ConnectionData = std::shared_ptr<int>;

class ENetBase;

class ENetBase {
 protected:
	ENetHost* host = nullptr;
	ProtocolInfo info;	// default

	/** all the connected clients with a data *
	std::set<ENetConnection> connections;

 public:	// event handlers
	/** handled every time a client connects. @warning does not triggered on a server disconnection process. *
	Subject<void, ENetBase&, ENetConnection&> h_connect;

	/** triggered when asuccesfully connected peer is disconnected. Does not work in force_disconnect() case.
	 * also it's not triggered on a peers, that does not respond with a succesful disconnection.
	 *
	Subject<void, ENetBase&, ENetConnection&> h_disconnect;

	/**
	 * Triggered every time a message is recieved
	 * @param basic
	 * @param basic
	 * @param int specifies channel id where the message was recieved
	 * @param string_view message data
	 */
	Subject<void, ENetBase&, ENetConnection&, int, const std::string_view> h_recieve;	 // channel_id, data

	/** server shutdown/disconnection handler. all connections are still valid.
	 * @warning does not called on force_disconnect at all!
	 */
	Subject<void, ENetBase&> h_shutdown;

	bool defer_destroy = false;

 protected:
	// handle events servicing
	void on_event_recv(ENetEvent& ev);

 protected:
	/** @warning do not use inside event handers! */
	void create_server() {
		if (host) std::runtime_error("host is already exists!");
		defer_destroy = false;
		auto addr = info.getAddress();
		host = enet_host_create(&addr, info.nconnections, info.nchannels, 0, 0);
	}

	/** @warning do not use inside event handers! */
	void create_client() {
		if (host) std::runtime_error("host is already exists!");
		defer_destroy = false;
		host = enet_host_create(NULL, info.nconnections, info.nchannels, 0, 0);
	}

	/** calls function at the first argument on all the peers connected to this host.*/
	void foreach (std::function<void(ENetPeer*)> cb) {
		if (!host) return;
		ENetPeer* currentPeer;
		for (currentPeer = host->peers; currentPeer < &host->peers[host->peerCount]; ++currentPeer) {
			cb(currentPeer);
		}
	}

	/** @warning do not use inside event handers! Use defer_destroy=true; instead!
	 * Also, this function does not invoke h_shutdown and h_disconnect events AT ALL!
	 */
	void force_destroy() {
		// cleanup all userdata
		foreach ([](ENetPeer* peer) {
			auto v = (ConnectionData*)enet_peer_get_data(peer);
			if (v) delete v;
			enet_peer_set_data(peer, nullptr);
		})
			;
		n_clients = 0;

		if (host) enet_host_destroy(host);
		host = nullptr;
	}

	/** @warning do not use inside event handers! Use defer_destroy=true; instead!
	 * Peacefully disconnects clients once, max 5s delay, 8 times more attempts.
	 * Clients that was not disconnected peacefully will be destroyed without h_desconnect event triggered!
	 */
	void destroy();

	/** @warning this does not affect already running Host! You should do destroy() and create()... maybe... */
	void set_address(ProtocolInfo i) { info = i; }

 public:
	ENetBase() {}
	ENetBase(const ENetBase&) = delete;
	ENetBase(ENetBase&&) = default;
	ENetBase& operator=(const ENetBase&) = delete;
	ENetBase& operator=(ENetBase&&) = default;
	ENetBase(ProtocolInfo i) : info(i) {}

	/** use this oeprator in order to check if server is terminated!
	 * You may try to reconnect after that or do delete class instance.
	 */
	operator bool() const { return !!host; }

	/**
	 * Same as operator bool().
	 */
	bool running() { return !!host; }

	/** does deletes ENet hosts in a way how force_destroy() does, if it wasn't done already. */
	virtual ~ENetBase() { force_destroy(); }

	/** flushes all packages to be finally sended! No need to do if you regulary call a service()! */
	void flush() {
		if (host) enet_host_flush(host);
	}

	/** all work is done internally, but you may optional get event, but cna't use any data!
	 * @param timeout in illiseconds
	 */
	bool service(ENetEvent* ev = nullptr, uint32_t timeout = 0) {
		ENetEvent _dummy;
		if (!ev) ev = &_dummy;

		/** runned in case destruction is deferred. that's right :p */
		if (defer_destroy) destroy();
		if (!host) {
			*ev = ENetEvent{ENET_EVENT_TYPE_NONE, 0, 0, 0, 0};
			return false;
		}

		bool stat = enet_host_service(host, ev, timeout) > 0;
		on_event_recv(*ev);
		return stat;
	}
};

class ENetClient : public Default, public ENetBase {
	ENetPeer* server = nullptr;

 public:
	ENetClient() {
		h_disconnect.subscribe([this](ENetBase&, ENetConnection& conn) {
			defer_destroy = true;	 // shutdown server
			server = nullptr;
			std::cerr << "Client: Server is disconnected!" << std::endl;
		});
	};

	ENetClient(ProtocolInfo info) : ENetClient() { set_address(info); }

	~ENetClient() {
		server = nullptr;
		// all the other stuff is done in the base class!
	}

	/** create AND connect a client to the specified server! */
	bool connect(const char* ip, unsigned short port = DEFAULT_PORT);

	/** function for a user :) */
	void disconnect() {
		if (server) {
			enet_peer_reset(server);
			server = nullptr;
		}
		destroy();	// destroy connection, all user data, etc.
	}

	/* same as running() + extra check */
	bool is_connected() { return running() && server != nullptr; }

	/** @warning channel starts FROM ZERO!!!! */
	bool send(uint8_t channel, ENetPacket* data) {
		if (!server || !host) return false;
		return enet_peer_send(server, channel, data);
	}

	/** @warning channel starts FROM ZERO!!!! */
	bool send(uint8_t channel, const std::string_view data) {
		if (!server || !host) return false;
		ENetPacket* p = enet_packet_create(data.data(), data.size(), 0);
		if (!p) return false;
		return send(channel, p);
	}
};

class ENetServer : public Default, public ENetBase {
 public:
	bool shutdown_on_leaving = false;

 public:
	ENetServer(ProtocolInfo info) {
		set_address(info);
		h_disconnect.subscribe([this](ENetBase&, ENetConnection& conn) {
			if (shutdown_on_leaving && host->peerCount <= 1) defer_destroy = true;	// shutdown server
			std::cerr << "Server: client is disconnected!" << std::endl;
		});
	}

	bool create() {
		ENetBase::create_server();
		return !!host;
	}

	void shutdown() {
		ENetBase::destroy();	// destroy connection, all user data, etc, and fire all callbacks
	}

	void destroy() {
		ENetBase::force_destroy();	// immediate destroy!
	}

	~ENetServer() { destroy(); }

	/** all work is done internally, but you may optional get event, but cna't use any data! */
	bool service(ENetEvent* ev = nullptr, int timeout = 1) {
		ENetEvent _dummy;
		if (!ev) ev = &_dummy;
		bool stat = enet_host_service(host, ev, timeout) > 0;
		on_event_recv(*ev);
		return stat;
	}

	void broadcast(uint8_t channel, ENetPacket* data) {
		if (host) enet_host_broadcast(host, channel, data);
	}

	void broadcast(uint8_t channel, const std::string_view v) {
		if (!host) return;
		ENetPacket* p = enet_packet_create(v.data(), v.size(), 0);
		if (!p) return;
		broadcast(channel, p);
	}

	void foreach (std::function<void(ENetConnection)> cb) {
		ENetBase::foreach ([cb](ENetPeer* p) { cb(ENetConnection(p)); });
	}
};

};	// namespace pb_old
#endif
