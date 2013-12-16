#include "net/client.h"
#include "net/server.h"
#include "net/protocol.h"
#include "net/client_funcs.h"
#include "lin_alg.h"
#include "car.h"
//#include "texture.h"

#include <string>

// client car colors
const vec4 colors[8] = {
	vec4(1.0, 0.2, 0.2, 1.0),
	vec4(0.4, 0.7, 0.2, 1.0),
	vec4(0.4, 0.2, 1.0, 1.0),
	vec4(1.0, 1.0, 0.0, 1.0),
	vec4(0.0, 1.0, 1.0, 1.0),
	vec4(1.0, 0.0, 1.0, 1.0),
	vec4(1.0, 1.0, 1.0, 1.0),
	vec4(0.0, 0.8, 0.2, 1.0)
};

//extern HeightMap *map;

LocalClient::ListenTaskThread LocalClient::Listener(NULL);
LocalClient::KeystateTaskThread LocalClient::KeystateManager(NULL);
LocalClient::PingmanagerTaskThread LocalClient::Ping(NULL);

Socket LocalClient::socket;
struct Client LocalClient::client;
struct sockaddr_in LocalClient::remote_sockaddr;
mutexed_peer_map LocalClient::peers;

std::string LocalClient::preferred_name = "Player";
int LocalClient::connected = 0;
double LocalClient::latency_ms = 0;
TIMER LocalClient::posupd_timer;
bool LocalClient::shutdown_requested = false;

unsigned LocalClient::latest_posupd_seq_number = 0;

unsigned short LocalClient::port = 50001;

static Car local_car;

static int net_bytes_out = 0;
static int net_bytes_in = 0;

int LocalClient::connect(const std::string &ip_port_string) {
	
	// establish what can called a virtual connection :P

	client.info.ip_string = "127.0.0.1";
	client.info.id = ID_CLIENT_UNASSIGNED;	// not assigned
	
	std::vector<std::string> tokens = split(ip_port_string, ':');

	if (tokens.size() != 2) {
		PRINT("connect: Error: badly formatted ip_port_string (%s)!\n", ip_port_string.c_str());
		return 0;
	}

	std::string remote_ip = tokens[0];
	unsigned short remote_port = std::stoi(tokens[1]);

	client.seq_number = 0;

	remote_sockaddr.sin_family = AF_INET;
	remote_sockaddr.sin_port = htons(remote_port);
//	remote_sockaddr.sin_addr.S_un.S_addr = inet_addr(remote_ip.c_str());
	remote_sockaddr.sin_addr.s_addr = inet_addr(remote_ip.c_str());

#ifdef _WIN32
	Socket::initialize();
#endif 

	socket = Socket(port, SOCK_DGRAM, true);
	
	client.address = socket.get_own_addr();	// :D

	if (socket.is_bad()) { 
		PRINT( "LocalClient: socket init failed.\n");
		LocalClient::request_shutdown();
		return 0;
	}

	PRINT("client: attempting to connect to %s:%u.\n", remote_ip.c_str(), remote_port);

	int success = handshake();
	if (!success) { 
		LocalClient::request_shutdown();
		return 0; 
	}

	Listener.start();
	KeystateManager.start();
	Ping.start();

	return 1;

}

void LocalClient::stop() {
	static const PTCLHEADERDATA TERMINATE_PACKET = { PROTOCOL_ID, 0, ID_SERVER, C_TERMINATE };
	// the packet of death. this sets connected = false and breaks out of the listen loop gracefully (ie. without calling WSACleanup) :D

	struct sockaddr_in own_addr = LocalClient::socket.get_own_addr();
	socket.send_data(&own_addr, (const char*)&TERMINATE_PACKET, sizeof(TERMINATE_PACKET));
	KeystateManager.stop();
	Listener.stop();
	Ping.stop();

	// VarTracker_untrack(LocalClient::latency_ms);	// nyi

	peers.clear();
	connected = 0;

	LocalClient::socket.destroy();

	shutdown_requested = false;
}

void LocalClient::KeystateTaskThread::task() {
	
	#define KEYSTATE_GRANULARITY_MS 46.66	// aiming for as low a rate as possible
	static TIMER keystate_timer;
	keystate_timer.begin();
	while (LocalClient::KeystateTaskThread::is_running() && LocalClient::is_connected()) {
		if (keystate_timer.get_ms() > KEYSTATE_GRANULARITY_MS) {
			//update_keystate(keys.pressed);
			post_keystate();
			keystate_timer.begin();
			long wait_ms = KEYSTATE_GRANULARITY_MS - keystate_timer.get_ms();
			if (wait_ms > 1) { SLEEP_MS(wait_ms); }
		}
	}
}

int LocalClient::handshake() {
	
	char handshake_buffer[PACKET_SIZE_MAX];

	PTCLHEADERDATA HANDSHAKE_HEADER
		= protocol_make_header(client.seq_number, ID_CLIENT_UNASSIGNED, C_HANDSHAKE);

	client.info.name = preferred_name;

	protocol_copy_header(handshake_buffer, &HANDSHAKE_HEADER);
	copy_to_ext_buffer(handshake_buffer, client.info.name.c_str(), client.info.name.length(), PTCL_HEADER_LENGTH);

	send_data_to_server(handshake_buffer, PTCL_HEADER_LENGTH + client.info.name.length());

			
#define RETRY_GRANULARITY_MS 1000
#define NUM_RETRIES 5
		
	PRINT( "LocalClient::sending handshake to remote.\n");
	
	// FIXME: currently broken when handshaking with localhost :P
	// the WS2 select() call reports "data available" when sending to 127.0.0.1, despite different port.

	int received_data = 0;
	for (int i = 0; i < NUM_RETRIES; ++i) {
		int select_r = socket.wait_for_incoming_data(RETRY_GRANULARITY_MS);
		if (select_r > 0) { 
			// could be wrong data. should check the data we received and retry if it wasn't the correct stuff
			received_data = 1;
			break;
		}
		PRINT("Re-sending handshake to remote...\n");
		send_data_to_server(handshake_buffer, PTCL_HEADER_LENGTH + client.info.name.length());
	}

	if (!received_data) {
		PRINT("Handshake timed out.\n");
		LocalClient::request_shutdown();
		return 0;
	}
	
	struct sockaddr_in from;
	int bytes = socket.receive_data(handshake_buffer, &from);

	PTCLHEADERDATA header;
	protocol_get_header_data(handshake_buffer, &header);

	if (header.protocol_id != PROTOCOL_ID) {
		PRINT( "LocalClient: handshake: protocol id mismatch (received %d)\n", header.protocol_id);
		LocalClient::request_shutdown();
		return 0;
	}

	const unsigned char &cmd = header.cmd_arg_mask.ch[0];

	if (cmd != S_HANDSHAKE_OK) {
		PRINT("LocalClient: handshake: received cmdbyte != S_HANDSHAKE_OK (%x). Handshake failed.\n", cmd);
		LocalClient::request_shutdown();
		return 0;
	}

	// get player id embedded within the packet
	copy_from_ext_buffer(handshake_buffer, &client.info.id, sizeof(client.info.id), PTCL_HEADER_LENGTH);

	// get player name received from server
	client.info.name = std::string(handshake_buffer + PTCL_HEADER_LENGTH + sizeof(client.info.id));
	PRINT("Client: received player id %d and name %s from %s. =)\n", client.info.id, client.info.name.c_str(), get_dot_notation_ipv4(&from).c_str());
	
	connected = true;

	// VarTracker_track_unit(double, LocalClient::latency_ms, "ms");
	return 1;

}

void LocalClient::PingmanagerTaskThread::ping() {
	PTCLHEADERDATA PING_HEADER = { PROTOCOL_ID, client.seq_number, client.info.id, C_PING };
	copy_to_buffer(VAR_SZ(PING_HEADER), 0);
	_latest_ping_seq_number = client.seq_number;
	send_data_to_server(buffer, PTCL_HEADER_LENGTH);
}

void LocalClient::PingmanagerTaskThread::task() {

#define PING_GRANULARITY_MS 2000
#define TIMEOUT_MS 5000
	timer.begin();

	while(LocalClient::PingmanagerTaskThread::is_running() && LocalClient::is_connected()) {
		if (time_since_last_ping_ms() > TIMEOUT_MS) {
			// gtfo?
		}
		ping();
		long wait_ms = PING_GRANULARITY_MS - timer.get_ms();
		SLEEP_MS(wait_ms);	// this needn't be precise
		timer.begin();
	}

}



static std::vector<struct peer_info_t> process_peer_list_string(const char* buffer) {
	std::vector<struct peer_info_t> peers;
	std::vector<std::string> sub = split(buffer, '|');
	auto it = sub.begin();
	while (it != sub.end()) {
		std::vector<std::string> tokens = split(*it, '/');
		if (tokens.size() != 4) {
			//PRINT( "warning: process_peer_list_string: tokens.size() != 3 (%d): dump = \"%s\"\n", tokens.size(), it->c_str());
		}
		else {
			peers.push_back(peer_info_t(std::stoi(tokens[0]), tokens[1], tokens[2], std::stoi(tokens[3])));
		}
		++it;
	}
	return peers;	
}


void LocalClient::send_chat_message(const std::string &msg) {
	if (!connected) { 
		PRINT("chat: not connected to server.\n");
		return; 
	}
	fprintf(stderr, "sending chat message \"%s\"\n", msg.c_str());

	static char chat_msg_buffer[PACKET_SIZE_MAX];
	
	int message_len = MIN(msg.length(), PTCL_PACKET_BODY_LEN_MAX);

	PTCLHEADERDATA CHAT_HEADER 
		= protocol_make_header(client.seq_number, client.info.id, C_CHAT_MESSAGE, message_len);
	
	protocol_copy_header(chat_msg_buffer, &CHAT_HEADER);

	memcpy(chat_msg_buffer + PTCL_HEADER_LENGTH, msg.c_str(), message_len);
	size_t total_size = PTCL_HEADER_LENGTH + message_len;
	chat_msg_buffer[PACKET_SIZE_MAX - 1] = '\0';
	send_data_to_server(chat_msg_buffer, total_size);
}

void LocalClient::ListenTaskThread::handle_current_packet() {

	PTCLHEADERDATA header;
	protocol_get_header_data(buffer, &header);

	if (header.protocol_id != PROTOCOL_ID) {
		PRINT( "dropping packet. Reason: protocol_id mismatch (%d)\n", header.protocol_id);
		return;
	}

	if (header.sender_id != ID_SERVER) {
		//PRINT( "unexpected sender id. expected ID_SERVER (%x), got %x instead.\n", ID_SERVER, sender_id);
		return;
	}

	const unsigned char &cmd = header.cmd_arg_mask.ch[0];

	switch (cmd) {

		case S_POSITION_UPDATE:
		// don't apply older pupd-package data. we also have an interpolation mechanism :P
			if (header.seq_number > latest_posupd_seq_number) {
				update_positions();
				latest_posupd_seq_number = header.seq_number;
			}
			break;

		case S_PONG: {
			latency_ms = LocalClient::Ping.time_since_last_ping_ms();
			unsigned embedded_seq_number;
			copy_from_buffer(&embedded_seq_number, sizeof(embedded_seq_number), PTCL_HEADER_LENGTH);
			if (embedded_seq_number != Ping._latest_ping_seq_number) {
				PRINT("Received S_PONG from server, but the embedded seq_number (%u) doesn't match with the expected one (%d).\n", embedded_seq_number, Ping._latest_ping_seq_number);
			}
			Ping._latest_ping_seq_number = 0; 
			break;
		}
		case S_CLIENT_CHAT_MESSAGE: {
			// the sender id is embedded into the datafield (bytes 12-14) of the packet
			unsigned short sender_id = 0;
			copy_from_buffer(&sender_id, sizeof(sender_id), PTCL_HEADER_LENGTH);
			auto it = peers.find(sender_id);
			if (it != peers.end()) {
			std::string chatmsg(buffer + PTCL_HEADER_LENGTH + sizeof(sender_id));
				if (!Server::is_running()) {
				PRINT("[%s] <%s>: %s\n", get_timestamp().c_str(), it->second.info.name.c_str(), chatmsg.c_str());
				}
			}
				else {
				// perhaps request a new peer list from the server. :P
				PRINT("warning: server broadcast S_CLIENT_CHAT_MESSAGE with unknown sender id %u!\n", sender_id);
			}
			break;
		}

		case S_SHUTDOWN:
			PRINT( "Client: Received S_SHUTDOWN from server (server going down).\n");
			LocalClient::request_shutdown(); // this will break from the recvfrom loop gracefully
			// this will cause LocalClient::stop() to be called from the main (rendering) thread
			break;

		case C_TERMINATE:
			PRINT("Client:Received C_TERMINATE from self. Stopping.\n");
			LocalClient::request_shutdown();
			break;
	
		case S_PEER_LIST:
			construct_peer_list();
		break;

		case S_CLIENT_DISCONNECT: {
			unsigned short id;
			copy_from_buffer(&id, sizeof(id), PTCL_HEADER_LENGTH);
			auto it = peers.find(id);
			if (it == peers.end()) {
				//PRINT( "Warning: received S_CLIENT_DISCONNECT with unknown id %u.\n", id);
			}
			else {
				PRINT( "Client %s (id %u) disconnected.\n", it->second.info.name.c_str(), id);
				peers.erase(id);
			}
			break;
		}
	
		default:
			PRINT("Warning: received unknown command char %u from server.\n", cmd);
			break;
	}

}

void LocalClient::ListenTaskThread::update_positions() {
	command_arg_mask_union cmd_arg_mask;
	copy_from_buffer(&cmd_arg_mask, PTCL_CMD_ARG_FIELD);
	unsigned num_clients = cmd_arg_mask.ch[1];

	static const size_t serial_data_size = sizeof(Car().data_serial);
	static const size_t PTCL_POS_DATA_SIZE = sizeof(unsigned short) + serial_data_size;

	size_t total_size = PTCL_HEADER_LENGTH;

	for (unsigned i = 0; i < num_clients; ++i) {
		unsigned short id;
		copy_from_buffer(&id, sizeof(id), PTCL_HEADER_LENGTH + i*PTCL_POS_DATA_SIZE);
		auto it = peers.find(id);
		
		if (it == peers.end()) {
			PRINT( "update_positions: unknown peer id included in peer list (%u)\n", id);
		}
		else {
			size_t offset = PTCL_HEADER_LENGTH + i*PTCL_POS_DATA_SIZE + sizeof(id);
			peers.update_peer_data(it, buffer + offset, serial_data_size);
		}
	}
	LocalClient::posupd_timer.begin();	
}

inline void interp(Car &car, float DT_COEFF) {
		float turn_coeff = turn_velocity_coeff(car.state.velocity);
		car.state.direction += (car.state.velocity > 0 ? turning_modifier_forward : turning_modifier_reverse)
								*car.state.front_wheel_angle*car.state.velocity*turn_coeff*DT_COEFF;
	

		car.state.wheel_rot -= 1.07*car.state.velocity * DT_COEFF;
		car.state.pos[0] += car.state.velocity*sin(car.state.direction-M_PI/2) * DT_COEFF;
		car.state.pos[2] += car.state.velocity*cos(car.state.direction-M_PI/2) * DT_COEFF;

}

void LocalClient::interpolate_positions() {
	if (peers.num_peers() <= 0) { 
		posupd_timer.begin();	// workaround, don't know what for though
		return; 
	}
	static const float HALF_GRANULARITY = POSITION_UPDATE_GRANULARITY_MS/2.0;
	const float DT_COEFF = HALF_GRANULARITY/16.666;

	//float local_DT_COEFF = time_since_last_posupd_ms()/16.666;

	auto it = peers.begin();
	while (it != peers.end()) {
		Car &car = it->second.car;
		interp(car, DT_COEFF);
		++it;
	}

}

void LocalClient::ListenTaskThread::task() {
	
	struct sockaddr_in from;

	static TIMER ping_timer;	// seems like a waste to actually create another thread for this
	ping_timer.begin();

#define PING_GRANULARITY_MS 2000
	
	while (LocalClient::ListenTaskThread::is_running() && LocalClient::is_connected()) {
		int bytes = socket.receive_data(buffer, &from);
		if (bytes > 0) { handle_current_packet(); }
	}

	post_quit_message();

}

void LocalClient::KeystateTaskThread::post_keystate() {
	PTCLHEADERDATA KEYSTATE_HEADER = { PROTOCOL_ID, client.seq_number, client.info.id, C_KEYSTATE };
	KEYSTATE_HEADER.cmd_arg_mask.ch[1] = client.keystate;
	copy_to_buffer(VAR_SZ(KEYSTATE_HEADER), 0);
	send_data_to_server(buffer, PTCL_HEADER_LENGTH);
}

void LocalClient::KeystateTaskThread::update_keystate(const bool *keys) {
	client.keystate = 0x0;

/*	if (keys[VK_UP]) { client.keystate |= C_KEYSTATE_UP; }
	if (keys[VK_DOWN]) { client.keystate |= C_KEYSTATE_DOWN; }
	if (keys[VK_LEFT]) { client.keystate |= C_KEYSTATE_LEFT; }
	if (keys[VK_RIGHT]) { client.keystate |= C_KEYSTATE_RIGHT; }*/
}

void LocalClient::ListenTaskThread::post_quit_message() {
	PTCLHEADERDATA QUIT_HEADER 
		= protocol_make_header( client.seq_number, client.info.id, C_QUIT);
	protocol_copy_header(buffer, &QUIT_HEADER);
	send_data_to_server(buffer, PTCL_HEADER_LENGTH);
}

void LocalClient::parse_user_input(const std::string s) {
	if (s.length() <= 0) { return; }
	if (s[0] != '/') {
		// - we're dealing with a chat message
		send_chat_message(s);
	}
	else {
		std::vector<std::string> input = split(s, ' ');
		if (input.size() <= 0) { return; }
		auto it = funcs.find(input[0]);
		if (it != funcs.end()) { 
			it->second(input);
		}
		else {
			PRINT("%s: command not found. See /help for a list of available commands.\n", input[0].c_str());
		}
		
	}
}

void LocalClient::quit() {
	LocalClient::stop();
	if (Server::is_running()) {
		Server::shutdown();
	}
	#ifdef _WIN32
	Socket::deinitialize();
	#endif
}

int LocalClient::send_data_to_server(const char* buffer, size_t size) {

	int bytes = socket.send_data(&remote_sockaddr, buffer, size);
	++client.seq_number;



	return bytes;
}
void LocalClient::set_name(const std::string &nick) {
	if (!connected) {
		preferred_name = nick;
		PRINT("Name set to %s.\n", nick.c_str());
	} else {
		PRINT("set_name: cannot change name while connected to server.\n");
	}
}

void LocalClient::ListenTaskThread::construct_peer_list() {
					
	std::vector<struct peer_info_t> peer_list = process_peer_list_string(buffer + PTCL_HEADER_LENGTH + 1);
		
	for (auto &it : peer_list) {
		//PRINT( "peer data: id = %u, name = %s, ip_string = %s\n", it.id, it.name.c_str(), it.ip_string.c_str());
		auto map_iter = peers.find(it.id);
		if (map_iter == peers.end()) {
			peers.insert(std::make_pair(it.id, Peer(it.id, it.name, it.ip_string, it.color)));
			PRINT("Client \"%s\" (id %u) connected from %s.\n", it.name.c_str(), it.id, it.ip_string.c_str());
		}
		else {
			if (!(map_iter->second.info == it)) {
				PRINT( "warning: peer was found in the peer list, but peer_info_t discrepancies were discovered.\n");
			}
		}
	}


}
