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
#include <base.hpp>
#include <exception>
#include <iostream>
#include <memory>
#include <set>
#include <stdexcept>
#include <string>
#include <functional>
#include <string_view>
#include "external/enet.h"
#include <string.h>
#include "base/event.hpp"

/**
 * @description ENet Wrapper
 */
namespace pb {

	static constexpr unsigned short DEFAULT_PORT = 4792;

	struct ProtocolInfo {
		const char* ip = "localhost";
		unsigned short port = DEFAULT_PORT;
		int nchannels = 5;
		int nconnections = 0;
		public:
		inline ENetAddress getAddress() {
			ENetAddress addr;
			addr.port = port;

			if (ip)
				enet_address_set_host(&addr, ip);	
			else addr.host = ENET_HOST_ANY;
			return addr;
		}
	};

	static void HostDefaultConfig(ENetHost* host) {
		// TODO Adjust to better values!
		host->maximumPacketSize = 1024 * 32;
		host->maximumWaitingData = 1024 * 128;
	}

	/** represents any data */
	using ConnectionData = std::shared_ptr<Abstract>;

	class ENetConnection {
		ENetPeer* peer = nullptr;
		public:
		ENetConnection(const ENetConnection&) = default;
		ENetConnection(ENetConnection&&) = default;
		ENetConnection& operator=(const ENetConnection&) = default;
		ENetConnection& operator=(ENetConnection&&) = default;

		explicit ENetConnection(ENetPeer* p) { peer = p;}
		~ENetConnection() {}	// nothing
		public:

		operator bool() {
			return peer != nullptr;
		}

		void disconnect() {
			if (!peer) return;
			enet_peer_disconnect(peer, 0);
		}

		void disconnect_later() {
			if (!peer) return;
			enet_peer_disconnect_later(peer, 0);
		}

		/** @warning you cannot use object methods after this! */
		void reset() { // forcefully disconnect
			if (!peer) return;
			{
				auto v = (ConnectionData*)enet_peer_get_data(peer);
				if (v) delete v;
				enet_peer_set_data(peer, nullptr);
			}
			enet_peer_reset(peer);
			peer = nullptr;
		}

		bool send(uint8_t channel, ENetPacket* data) {
			return enet_peer_send(peer, channel, data) == 0;
		}

		bool send(uint8_t channel, std::string_view data) {
			ENetPacket* p = enet_packet_create(data.data(), data.size(), 0);
			if (!p) return false;
			enet_peer_send(peer, channel, p);
		}

		ConnectionData& data() {
			auto v = (ConnectionData*)enet_peer_get_data(peer);
			if (!v) std::runtime_error("Connection has no associated data. Use afetr reset()/release_data() or misuse");
			return *v;
		}
	};

	/** @warning NEVER EVER TRY TO DESTROY ENETHOST FROM THE  EVENT HANDLER!
	 * PLEASE set defer_destroy to true if you want to do that instead!
	 * @warning NEVER try to reconnect/dsconnect or do any other stuff in a shutdown handler too!
	 * @warning PLEASE DO NOT USE CALLBACKS FOR RESOURCE DEINITIALISATION! Use userdata object destructor for that!
	 */
	class ENetBase {
		protected:
		ENetHost* host = nullptr;
		ProtocolInfo info; // default
		bool defer_destroy = false;

		public: // event handlers
		/** handled every time a client connects. @warning does not triggered on a server disconnection process. */
		Subject<void, ENetBase&, ENetConnection&> h_connect;

		/** triggered when asuccesfully connected peer is disconnected. Does not work in force_disconnect() case. 
		 * also it's not triggered on a peers, that does not respond with a succesful disconnection.
		 */
		Subject<void, ENetBase&, ENetConnection&> h_disconnect;

		/**
		 * Triggered every time a message is recieved
		 * @param basic
		 * @param basic
		 * @param int specifies channel id where the message was recieved
		 * @param string_view message data
		 */
		Subject<void, ENetBase&, ENetConnection&, int, const std::string_view> h_recieve; // channel_id, data

		/** server shutdown/disconnection handler. all connections are still valid.
		 * @warning does not called on force_disconnect at all!
		 */
		Subject<void, ENetBase&> h_shutdown; 

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
		void foreach(std::function<void(ENetPeer*)> cb) {
			if (!host) return;
			ENetPeer *currentPeer;
			for (currentPeer = host->peers;
				currentPeer < &host->peers[host->peerCount]; ++currentPeer) {
				cb(currentPeer);
			}
		}

		/** @warning do not use inside event handers! Use defer_destroy=true; instead! 
		 * Also, this function does not invoke h_shutdown and h_disconnect events AT ALL!
		 */
		void force_destroy() {
			// cleanup all userdata
			foreach([](ENetPeer* peer){
				auto v = (ConnectionData*)enet_peer_get_data(peer);
				if (v) delete v;
				enet_peer_set_data(peer, nullptr);
			});

			if (host) enet_host_destroy(host);
			host = nullptr;
		}

		/** @warning do not use inside event handers! Use defer_destroy=true; instead! 
		 * Peacefully disconnects clients once, max 5s delay, 8 times more attempts.
		 * Clients that was not disconnected peacefully will be destroyed without h_desconnect event triggered!
		 */
		void destroy();

		/** @warning this does not affect already running Host! You should do destroy() and create()... maybe... */
		void set_address(ProtocolInfo i) {
			info = i;
		}

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
		operator bool() const {
			return !!host;
		}

		/**
		 * Same as operator bool().
		 */
		bool running() {return !!host;}

		/** does deletes ENet hosts in a way how force_destroy() does, if it wasn't done already. */
		~ENetBase() {
			force_destroy(); 
		}

		/** flushes all packages to be finally sended! No need to do if you regulary call a service()! */
		void flush() {
			enet_host_flush(host);
		}

		/** all work is done internally, but you may optional get event, but cna't use any data!
		 * @param timeout in illiseconds 
		 */
		bool service(ENetEvent* ev = nullptr, uint32_t timeout = 0) { 
			ENetEvent _dummy;
			if (!ev) ev = &_dummy;

			/** runned in case destruction is deferred. that's right :p */
			if (defer_destroy) destroy();
			if (!host) {*ev = ENetEvent{ENET_EVENT_TYPE_NONE, 0, 0 , 0, 0}; return false;}

			bool stat = enet_host_service(host, ev, timeout) > 0;	
			on_event_recv(*ev);
			return stat;
		}

	};

	class ENetClient : public Default, ENetBase {
		ENetPeer* server = nullptr;
		public:

		ENetClient() {
			h_disconnect.subscribe([this](ENetBase&, ENetConnection& conn) {
				defer_destroy = true; // shutdown server
				server = nullptr;
				std::cerr << "Client: Server is disconnected!" << std::endl;
			});
		};

		ENetClient(ProtocolInfo info) : ENetClient() {
			set_address(info);
		}

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
			destroy(); // destroy connection, all user data, etc.
		}

		/* same as running() + extra check */
		bool is_connected() {
			return running() && server != nullptr;
		}

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

	class ENetServer : public Default,ENetBase {
		bool shutdown_on_leaving = false;
		public:
		ENetServer(ProtocolInfo info) {
			set_address(info);
		}

		bool create() {
			ENetBase::create_server();
			return !!host;
		}

		void shutdown() {
			ENetBase::destroy(); // destroy connection, all user data, etc, and fire all callbacks
		}

		void destroy() {
			ENetBase::force_destroy(); // immediate destroy!
		}

		~ENetServer() {
			destroy();
		}

		/** all work is done internally, but you may optional get event, but cna't use any data! */
		bool service(ENetEvent* ev = nullptr, int timeout = 1) {
			ENetEvent _dummy;
			if (!ev) ev = &_dummy;
			bool stat = enet_host_service(host, ev, timeout) > 0;	
			on_event_recv(*ev);
			return stat;
		}

		void broadcast(uint8_t channel, ENetPacket* data) {
			if (host)
			enet_host_broadcast(host, channel, data);
		}

		void foreach(std::function<void(ENetConnection)> cb) {
			ENetBase::foreach([cb](ENetPeer* p){
				cb(ENetConnection(p));
			});
		}
	};

};

