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
#include <string.h>
#include <assert.h>

#include <functional>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string_view>
#include <set>

#include "base/base.hpp"
#include "base/event.hpp"
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

class ENetPeerImpl {
 protected:
	::ENetPeer* peer = nullptr;
	ENetPeerImpl(::ENetPeer* ptr) : peer(ptr) {assert(peer != nullptr);};
	friend class ENetBase;

 public:
	ENetPeerImpl(const ENetPeerImpl&) = delete;
	ENetPeerImpl(ENetPeerImpl&&) = delete;
	ENetPeerImpl& operator=(const ENetPeerImpl&) = delete;
	ENetPeerImpl& operator=(ENetPeerImpl&&) = delete;

 public: // virtual methods
	virtual ~ENetPeerImpl() {}
	virtual void on_disconnect(bool timeouted) = 0; // called before potential destruction
	virtual void on_recieve(uint8_t channel, std::string_view data) = 0;

private: // handler caller
	void h_disconnect(bool fire_event = true) {
		assert(peer != nullptr);
		if (fire_event) this->on_disconnect(0);
		peer->data = nullptr;
	}

	void h_reset() {
		assert(peer != nullptr);
		peer = nullptr;
		delete this;
	}

 public:	// methods
	operator bool() { return peer ? peer->state == ENET_PEER_STATE_CONNECTED : false;}
	
	/** @warning this object will be destructed after this and memory freed! 
	 *  do not use object after calling this function.
	 *  on_disconnect callback is not called, client would not know about losing a connection.
	 */
	void reset() {
		h_disconnect(false);
		enet_peer_reset(peer);
		h_reset();
	}

	/** @warning this object will be destructed after this and memory freed!
	 * do not use object after calling this function.
	 * triggers on_disconect event
	 */
	void disconnect_now() {
		h_disconnect(true);
		enet_peer_disconnect_now(peer, 0);
		h_reset();
	}

	/** @warning do not call this inside errorhandler
	 */
	void disconnect() {
		assert(peer != nullptr);
		enet_peer_disconnect(peer, 0);
	}

	/** will call on_disconnect when all packets from and to client are done.
	 */
	void disconnect_later() {
		assert(peer != nullptr);
		enet_peer_disconnect_later(peer, 0);
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

	bool operator<(const ENetPeerImpl& with) { return peer < with.peer; }
};

using ENetConnection = std::reference_wrapper<ENetPeerImpl>;

class ENetBase {
	protected:
	ENetHost* host;
	ProtocolInfo info;

	// if we should shutdown connection after loop
	bool defer_disconnect = false;

	// prevents any peer to connecting to this server
	bool disable_connections = false;

	void on_event_recv(ENetEvent& ev);
	public:

	bool create_server() {
		if (host) std::runtime_error("host is already exists!");
		defer_disconnect = false; disable_connections = false;
		auto addr = info.getAddress();
		host = enet_host_create(&addr, info.nconnections, info.nchannels, 0, 0);
		return host != nullptr;
	}

	bool create_client() {
		if (host) std::runtime_error("host is already exists!");
		defer_disconnect = false; disable_connections = false;
		host = enet_host_create(NULL, info.nconnections, info.nchannels, 0, 0);
		return host != nullptr;
	}

	operator bool() {return host != nullptr;}

	/** calls function at the first argument on all the peers connected to this host.*/
	void foreach (std::function<void(ENetPeerImpl&)> cb) {
		assert(host != nullptr);
		ENetPeer* currentPeer;

		for (currentPeer = host->peers; currentPeer < &host->peers[host->peerCount]; ++currentPeer) {
			if (currentPeer->state != ENET_PEER_STATE_CONNECTED) continue;
			ENetPeerImpl* data = static_cast<ENetPeerImpl*>(currentPeer->data);
			assert(data != nullptr);
			cb(*data);
		}
	}

};

};	// namespace pb

#if 0
/**
 * @description ENet Wrapper
 *
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

class ENetConnection {
	ENetPeer* peer = nullptr;
	ConnectionData u_data;
	friend class ENetBase;

 public:
	/** please do not use in your app  :( *
	explicit ENetConnection(ENetPeer* p) { peer = p; }
	ENetConnection(const ENetConnection&) = default;
	ENetConnection(ENetConnection&&) = default;
	ENetConnection& operator=(const ENetConnection&) = default;
	ENetConnection& operator=(ENetConnection&&) = default;
	~ENetConnection() {}	// nothing
 public:
	operator bool() { return peer != nullptr; }

	void disconnect() {
		if (!peer) return;
		enet_peer_disconnect(peer, 0);
	}

	void disconnect_later() {
		if (!peer) return;
		enet_peer_disconnect_later(peer, 0);
	}

	/*
	* @warning you cannot use object methods after this!
	void reset() { // forcefully disconnect
		if (!peer) return;
		enet_peer_reset(peer);
		peer = nullptr;
	} *

	bool send(uint8_t channel, ENetPacket* data) { return enet_peer_send(peer, channel, data) == 0; }

	bool send(uint8_t channel, std::string_view data) {
		ENetPacket* p = enet_packet_create(data.data(), data.size(), 0);
		if (!p) return false;
		enet_peer_send(peer, channel, p);
		return true;
	}

	ConnectionData& data() { return u_data; }

	ENetPeer* raw() { return peer; }

	bool operator<(const ENetConnection& with) { return peer < with.peer; }
};

/** @warning NEVER EVER TRY TO DESTROY ENETHOST FROM THE  EVENT HANDLER!
 * PLEASE set defer_destroy to true if you want to do that instead!
 * @warning NEVER try to reconnect/dsconnect or do any other stuff in a shutdown handler too!
 * @warning PLEASE DO NOT USE CALLBACKS FOR RESOURCE DEINITIALISATION! Use userdata object destructor for that!
 *
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
