/*************************************************************************/
/*  websocket_server.cpp                                                 */
/*************************************************************************/
/*                       This file is part of:                           */
/*                           GODOT ENGINE                                */
/*                      https://godotengine.org                          */
/*************************************************************************/
/* Copyright (c) 2007-2022 Juan Linietsky, Ariel Manzur.                 */
/* Copyright (c) 2014-2022 Godot Engine contributors (cf. AUTHORS.md).   */
/*                                                                       */
/* Permission is hereby granted, free of charge, to any person obtaining */
/* a copy of this software and associated documentation files (the       */
/* "Software"), to deal in the Software without restriction, including   */
/* without limitation the rights to use, copy, modify, merge, publish,   */
/* distribute, sublicense, and/or sell copies of the Software, and to    */
/* permit persons to whom the Software is furnished to do so, subject to */
/* the following conditions:                                             */
/*                                                                       */
/* The above copyright notice and this permission notice shall be        */
/* included in all copies or substantial portions of the Software.       */
/*                                                                       */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,       */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF    */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.*/
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY  */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,  */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE     */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                */
/*************************************************************************/

#include "websocket_server.h"

GDCINULL(WebSocketServer);

WebSocketServer::WebSocketServer() {
	_peer_id = 1;
	bind_ip = IPAddress("*");
}

WebSocketServer::~WebSocketServer() {
}

void WebSocketServer::_bind_methods() {
	ClassDB::bind_method(D_METHOD("is_listening"), &WebSocketServer::is_listening);
	ClassDB::bind_method(D_METHOD("set_extra_headers", "headers"), &WebSocketServer::set_extra_headers, DEFVAL(Vector<String>()));
	ClassDB::bind_method(D_METHOD("listen", "port", "protocols", "gd_mp_api"), &WebSocketServer::listen, DEFVAL(Vector<String>()), DEFVAL(false));
	ClassDB::bind_method(D_METHOD("stop"), &WebSocketServer::stop);
	ClassDB::bind_method(D_METHOD("has_peer", "id"), &WebSocketServer::has_peer);
	ClassDB::bind_method(D_METHOD("get_peer_address", "id"), &WebSocketServer::get_peer_address);
	ClassDB::bind_method(D_METHOD("get_peer_port", "id"), &WebSocketServer::get_peer_port);
	ClassDB::bind_method(D_METHOD("disconnect_peer", "id", "code", "reason"), &WebSocketServer::disconnect_peer, DEFVAL(1000), DEFVAL(""));

	ClassDB::bind_method(D_METHOD("get_bind_ip"), &WebSocketServer::get_bind_ip);
	ClassDB::bind_method(D_METHOD("set_bind_ip", "ip"), &WebSocketServer::set_bind_ip);
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "bind_ip"), "set_bind_ip", "get_bind_ip");

	ClassDB::bind_method(D_METHOD("get_private_key"), &WebSocketServer::get_private_key);
	ClassDB::bind_method(D_METHOD("set_private_key", "key"), &WebSocketServer::set_private_key);
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "private_key", PROPERTY_HINT_RESOURCE_TYPE, "CryptoKey", PROPERTY_USAGE_NONE), "set_private_key", "get_private_key");

	ClassDB::bind_method(D_METHOD("get_tls_certificate"), &WebSocketServer::get_tls_certificate);
	ClassDB::bind_method(D_METHOD("set_tls_certificate", "cert"), &WebSocketServer::set_tls_certificate);
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "tls_certificate", PROPERTY_HINT_RESOURCE_TYPE, "X509Certificate", PROPERTY_USAGE_NONE), "set_tls_certificate", "get_tls_certificate");

	ClassDB::bind_method(D_METHOD("get_ca_chain"), &WebSocketServer::get_ca_chain);
	ClassDB::bind_method(D_METHOD("set_ca_chain", "ca_chain"), &WebSocketServer::set_ca_chain);
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "ca_chain", PROPERTY_HINT_RESOURCE_TYPE, "X509Certificate", PROPERTY_USAGE_NONE), "set_ca_chain", "get_ca_chain");

	ClassDB::bind_method(D_METHOD("get_handshake_timeout"), &WebSocketServer::get_handshake_timeout);
	ClassDB::bind_method(D_METHOD("set_handshake_timeout", "timeout"), &WebSocketServer::set_handshake_timeout);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "handshake_timeout"), "set_handshake_timeout", "get_handshake_timeout");

	ADD_SIGNAL(MethodInfo("client_close_request", PropertyInfo(Variant::INT, "id"), PropertyInfo(Variant::INT, "code"), PropertyInfo(Variant::STRING, "reason")));
	ADD_SIGNAL(MethodInfo("client_disconnected", PropertyInfo(Variant::INT, "id"), PropertyInfo(Variant::BOOL, "was_clean_close")));
	ADD_SIGNAL(MethodInfo("client_connected", PropertyInfo(Variant::INT, "id"), PropertyInfo(Variant::STRING, "protocol"), PropertyInfo(Variant::STRING, "resource_name")));
	ADD_SIGNAL(MethodInfo("data_received", PropertyInfo(Variant::INT, "id")));
}

IPAddress WebSocketServer::get_bind_ip() const {
	return bind_ip;
}

void WebSocketServer::set_bind_ip(const IPAddress &p_bind_ip) {
	ERR_FAIL_COND(is_listening());
	ERR_FAIL_COND(!p_bind_ip.is_valid() && !p_bind_ip.is_wildcard());
	bind_ip = p_bind_ip;
}

Ref<CryptoKey> WebSocketServer::get_private_key() const {
	return private_key;
}

void WebSocketServer::set_private_key(Ref<CryptoKey> p_key) {
	ERR_FAIL_COND(is_listening());
	private_key = p_key;
}

Ref<X509Certificate> WebSocketServer::get_tls_certificate() const {
	return tls_cert;
}

void WebSocketServer::set_tls_certificate(Ref<X509Certificate> p_cert) {
	ERR_FAIL_COND(is_listening());
	tls_cert = p_cert;
}

Ref<X509Certificate> WebSocketServer::get_ca_chain() const {
	return ca_chain;
}

void WebSocketServer::set_ca_chain(Ref<X509Certificate> p_ca_chain) {
	ERR_FAIL_COND(is_listening());
	ca_chain = p_ca_chain;
}

float WebSocketServer::get_handshake_timeout() const {
	return handshake_timeout / 1000.0;
}

void WebSocketServer::set_handshake_timeout(float p_timeout) {
	ERR_FAIL_COND(p_timeout <= 0.0);
	handshake_timeout = p_timeout * 1000;
}

MultiplayerPeer::ConnectionStatus WebSocketServer::get_connection_status() const {
	if (is_listening()) {
		return CONNECTION_CONNECTED;
	}

	return CONNECTION_DISCONNECTED;
}

bool WebSocketServer::is_server() const {
	return true;
}

void WebSocketServer::_on_peer_packet(int32_t p_peer_id) {
	if (_is_multiplayer) {
		_process_multiplayer(get_peer(p_peer_id), p_peer_id);
	} else {
		emit_signal(SNAME("data_received"), p_peer_id);
	}
}

void WebSocketServer::_on_connect(int32_t p_peer_id, String p_protocol, String p_resource_name) {
	if (_is_multiplayer) {
		// Send add to clients
		_send_add(p_peer_id);
		emit_signal(SNAME("peer_connected"), p_peer_id);
	} else {
		emit_signal(SNAME("client_connected"), p_peer_id, p_protocol, p_resource_name);
	}
}

void WebSocketServer::_on_disconnect(int32_t p_peer_id, bool p_was_clean) {
	if (_is_multiplayer) {
		// Send delete to clients
		_send_del(p_peer_id);
		emit_signal(SNAME("peer_disconnected"), p_peer_id);
	} else {
		emit_signal(SNAME("client_disconnected"), p_peer_id, p_was_clean);
	}
}

void WebSocketServer::_on_close_request(int32_t p_peer_id, int p_code, String p_reason) {
	emit_signal(SNAME("client_close_request"), p_peer_id, p_code, p_reason);
}
