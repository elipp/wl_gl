#include "net/server.h"
#include "net/client.h"
#include "timer.h"

#include <cmath>

static const float FW_ANGLE_LIMIT = M_PI/6; // rad
static const float SQRT_FW_ANGLE_LIMIT_RECIP = 1.0/sqrt(FW_ANGLE_LIMIT);
static const float REAL_ANGLE_LIMIT = 0.6*FW_ANGLE_LIMIT;
// f(x) = -(1/(x+1/sqrt(30))^2) + 30
// f(x) = 1/(-x + 1/sqrt(30))^2 - 30


static float f_wheel_angle(float x) {
	if (x >= 0) {
		float t = (x + SQRT_FW_ANGLE_LIMIT_RECIP);
		float angle = -1/(t*t) + FW_ANGLE_LIMIT;
		return angle > REAL_ANGLE_LIMIT ? REAL_ANGLE_LIMIT : angle;
	}
	else {
		float t = (-x + SQRT_FW_ANGLE_LIMIT_RECIP);
		float angle = 1/(t*t) - FW_ANGLE_LIMIT;
		return angle < -REAL_ANGLE_LIMIT ? -REAL_ANGLE_LIMIT : angle;
	}
}

unsigned Server::seq_number = 0;

Server::Listen Server::Listener(NULL);	// use NULL callbacks
Server::Ping Server::PingManager(NULL);
Server::GameState Server::GameStateManager(NULL);

std::unordered_map<unsigned short, struct Client> Server::clients;
unsigned Server::num_clients = 0;
int Server::running = 0;
Socket Server::socket;

void Server::Listen::task(void) {

	while (Server::is_running() && Server::Listen::is_running()) {
		sockaddr_in from;
		int bytes = socket.receive_data(buffer, &from);

		if (bytes > 0) {
			handle_current_packet(&from);
		}
	}
	
	SERVER_PRINT("Server: stopping ListenTaskThread.\n");
}

int Server::init(unsigned short port) {
#ifdef _WIN32
	Socket::initialize();
#endif
	socket = Socket(port, SOCK_DGRAM, true); // blocking udp socket
	if (socket.is_bad()) { SERVER_PRINT( "Server::init, socket init error.\n"); return 0; }

	Listener.start();
	PingManager.start();
	GameStateManager.start();

	running = 1;
	SERVER_PRINT("\nServer: init successful. Bound to port %u.\n", get_port());
	return 1;
}

void Server::send_packet_of_death() {

	char dbuffer[128];

	static const PTCLHEADERDATA PACKET_OF_DEATH_HEADER = { PROTOCOL_ID, SEQN_ASSIGNED_ELSEWHERE, ID_SERVER, S_PACKET_OF_DEATH };
	memcpy(dbuffer, &PACKET_OF_DEATH_HEADER, sizeof(PACKET_OF_DEATH_HEADER)); 

	struct sockaddr_in self_addr = Server::socket.get_own_addr();
	Server::socket.send_data(&self_addr, dbuffer, PTCL_HEADER_LENGTH);

}

void Server::shutdown() {
	// post quit messages to all clients
	running = 0;
	SERVER_PRINT("Server: joining worker threads.\n");

	Server::send_packet_of_death();

	GameStateManager.stop();
	PingManager.stop();
	Listener.stop();

	clients.clear();
	Server::socket.destroy();
}


void Server::Listen::handle_current_packet(struct sockaddr_in *from) {

	PTCLHEADERDATA header;
	protocol_get_header_data(buffer, &header);

	if (header.protocol_id != PROTOCOL_ID) {
		SERVER_PRINT( "dropping packet. Reason: protocol_id mismatch (%x)!\nDump:\n", header.protocol_id);
		return;
	}

	auto client_iter = clients.find(header.sender_id);

	if (header.sender_id != ID_CLIENT_UNASSIGNED && header.sender_id != ID_SERVER) {
		if (client_iter == clients.end()) {
			SERVER_PRINT( "Server: warning: received packet from non-existing player-id %u. Ignoring.\n", header.sender_id);
			return;
		}
	}

	const unsigned char &cmd = header.cmd_arg_mask.ch[0];
	const unsigned char &cmd_arg = header.cmd_arg_mask.ch[1];

	std::string ip = get_dot_notation_ipv4(from);
	//SERVER_PRINT( "Received packet with size %u from %s (id = %u). Seq_number = %u\n", socket.current_data_length_in(), ip.c_str(), client_id, seq);

	switch (cmd) {
		case C_KEYSTATE:
			//if (seq > it->second.seq_number) {
			client_iter->second.keystate = cmd_arg;
			//}
			break;

		case C_PING: 
			client_iter->second.ping_timer.begin();
			pong(client_iter->second, header.seq_number);
			break; 

		case C_HANDSHAKE: {
			  std::string name_string(buffer + PTCL_HEADER_LENGTH);
			  static unsigned rename = 0;

			  for (auto &name_iter : clients) {
				  if (name_iter.second.info.name == name_string) { 
					  SERVER_PRINT("Client with name %s already found. Renaming.\n", name_string.c_str());
					  name_string = name_string + "(" + std::to_string(rename) + ")";
					  ++rename;
					  break; 
				  }
			  }

			  // its rather backward to add the client to the map before the handshake is actually sent :D
			  auto &newcl_ref = add_client(from, name_string)->second;
			  confirm_handshake(newcl_ref);
			  post_peer_list();
			  break;
		}
		case C_CHAT_MESSAGE: {
			     const std::string msg(buffer + PTCL_HEADER_LENGTH);	// lines like these are rather risky though.. :D
			     SERVER_PRINT("[%s] <%s>: %s\n", get_timestamp().c_str(), client_iter->second.info.name.c_str(), msg.c_str());
			     distribute_chat_message(msg, header.sender_id);
			     break;
	     	}
		case C_QUIT: 
			remove_client(client_iter);
		   	post_client_disconnect(header.sender_id);
		   	SERVER_PRINT("Server: received C_QUIT from client %d\n", header.sender_id);
			break;

		case S_PACKET_OF_DEATH:
			// fprintf(stderr, "Server: received S_PACKET_OF_DEATH!\n");
			Server::running = 0;
			if (Server::get_num_clients() > 0) {
				Server::broadcast_shutdown_message();
			}
			break;

		default:
			SERVER_PRINT("warning: received unrecognized cmdbyte %u with arg %u\n", cmd, cmd_arg);
			break;

	}

}

void Server::Listen::pong(struct Client &c, unsigned seq_number) {
	char pong_buffer[16];

	static PTCLHEADERDATA PING_HEADER = { PROTOCOL_ID, SEQN_ASSIGNED_ELSEWHERE, ID_SERVER, S_PONG };
	int accum_offset = copy_to_ext_buffer(pong_buffer, &PING_HEADER, sizeof(PING_HEADER), 0);
	accum_offset += copy_to_ext_buffer(pong_buffer, &seq_number, sizeof(seq_number), accum_offset);

	send_data_to_client(c, pong_buffer, accum_offset);

}

void Server::Listen::distribute_chat_message(const std::string &msg, const unsigned short sender_id) {

	static PTCLHEADERDATA CHAT_MESSAGE_HEADER = { PROTOCOL_ID, SEQN_ASSIGNED_ELSEWHERE, ID_SERVER, S_CLIENT_CHAT_MESSAGE };
	CHAT_MESSAGE_HEADER.cmd_arg_mask.ch[1] = (unsigned char)msg.length();

	size_t accum_offset = copy_to_buffer(&CHAT_MESSAGE_HEADER, sizeof(CHAT_MESSAGE_HEADER), 0);

	accum_offset += copy_to_buffer(VAR_SZ(sender_id), accum_offset);
	accum_offset += copy_to_buffer(msg.c_str(), msg.length(), accum_offset);

	send_data_to_all(buffer, accum_offset);
}

void Server::Listen::post_client_connect(const struct Client &c) {
	static PTCLHEADERDATA CLIENT_CONNECT_HEADER = { PROTOCOL_ID, SEQN_ASSIGNED_ELSEWHERE, ID_SERVER, S_CLIENT_CONNECT };
	copy_to_buffer(VAR_SZ(CLIENT_CONNECT_HEADER), 0);
	char buffer[128];

#ifdef _WIN32
	int bytes= sprintf_s(buffer, sizeof(buffer), "%u/%s/%s/%u", c.info.id, c.info.name.c_str(), c.info.ip_string.c_str(), c.info.color);
#elif __linux__
	int bytes= snprintf(buffer, sizeof(buffer), "%u/%s/%s/%u", c.info.id, c.info.name.c_str(), c.info.ip_string.c_str(), c.info.color);
#endif

	buffer[bytes-1] = '\0';
	copy_to_buffer(buffer, bytes, PTCL_HEADER_LENGTH);
	send_data_to_all(buffer, PTCL_HEADER_LENGTH + bytes);
}

id_client_map::iterator Server::Listen::add_client(struct sockaddr_in *newclient_saddr, const std::string &name) {
	++num_clients;
	static uint8_t color = 0;

	struct Client newclient;
	newclient.address = *newclient_saddr;
	newclient.info.id = num_clients;
	newclient.info.name = name;
	newclient.info.color = color;
	newclient.active_ping_seq_number = 0;
	newclient.seq_number = 0;

	color = color > 7 ? 0 : (color+1);

	newclient.info.ip_string = get_dot_notation_ipv4(newclient_saddr);

	auto k = clients.emplace(std::make_pair(newclient.info.id, newclient));

	SERVER_PRINT("Server: added client \"%s\" with id %d\n", newclient.info.name.c_str(), newclient.info.id);

	return k.first;
}


void Server::Listen::post_client_disconnect(unsigned short id) {

	static PTCLHEADERDATA CLIENT_DISCONNECT_HEADER = { PROTOCOL_ID, SEQN_ASSIGNED_ELSEWHERE, ID_SERVER, S_CLIENT_DISCONNECT };
	copy_to_buffer(VAR_SZ(CLIENT_DISCONNECT_HEADER), 0); 
	copy_to_buffer(VAR_SZ(id), PTCL_HEADER_LENGTH);

	send_data_to_all(buffer, PTCL_HEADER_LENGTH + sizeof(id));
}

void Server::send_data_to_all(char *buffer, size_t size) {

	for (auto &it : clients) {
		struct Client &c = it.second;
		protocol_update_seq_number(buffer, c.seq_number);
		send_data_to_client(c, buffer, size);
	}

}

id_client_map::iterator Server::remove_client(id_client_map::iterator &iter) {
	id_client_map::iterator it;
	if (iter != clients.end()) {
		it = clients.erase(iter);
		return it;
	}
	else {
		SERVER_PRINT("Server: remove client: internal error: couldn't find client %u from the client_id map (this shouldn't be happening)!\n", iter->second.info.id);
		return clients.end();
	}
}

void Server::Listen::confirm_handshake(struct Client &client) {

	PTCLHEADERDATA HANDSHAKE_HEADER = 
		protocol_make_header( SEQN_ASSIGNED_ELSEWHERE, ID_SERVER, S_HANDSHAKE_OK, client.info.color);

	int accum_offset = protocol_copy_header(buffer, &HANDSHAKE_HEADER);
	accum_offset += copy_to_buffer(VAR_SZ(client.info.id), accum_offset);
	accum_offset += copy_to_buffer(client.info.name.c_str(), client.info.name.length(), accum_offset);
	send_data_to_client(client, buffer, accum_offset);

}

void Server::Listen::post_peer_list() {

	PTCLHEADERDATA PEER_LIST_HEADER = 
		protocol_make_header( SEQN_ASSIGNED_ELSEWHERE, ID_SERVER, S_PEER_LIST, clients.size());
	protocol_copy_header(buffer, &PEER_LIST_HEADER);

	char peer_buf[512];	// FIXME: HORRIBLE WORKAROUND (fix intermittent stack corruption errors at sprintf_s)
	size_t offset = 0;

	id_client_map::iterator iter = clients.begin();
	while (iter != clients.end()) {
		struct Client &c = iter->second;

#ifdef _WIN32
		int bytes = sprintf_s(peer_buf + offset, PTCL_PACKET_BODY_LEN_MAX, "|%u/%s/%s/%u", c.info.id, c.info.name.c_str(), c.info.ip_string.c_str(), c.info.color);
#elif __linux__
		int bytes = snprintf(peer_buf + offset, PTCL_PACKET_BODY_LEN_MAX, "|%u/%s/%s/%u", c.info.id, c.info.name.c_str(), c.info.ip_string.c_str(), c.info.color);
#endif
		offset += bytes;
		if (offset >= PTCL_PACKET_BODY_LEN_MAX - 1) { 
			SERVER_PRINT( "Server: post_peer_list: warning: offset > max!\n"); 
			offset = PTCL_PACKET_BODY_LEN_MAX - 1; 
			break; 
		}
		++iter;
	}
	peer_buf[offset] = '\0';
	copy_to_buffer(peer_buf, offset, PTCL_HEADER_LENGTH);

	SERVER_PRINT( "Server: sending peer list to all clients.\n");

	send_data_to_all(buffer, PTCL_HEADER_LENGTH + offset);
}

void Server::broadcast_shutdown_message() {
	char sbuffer[128];
	PTCLHEADERDATA SHUTDOWN_HEADER = protocol_make_header(SEQN_ASSIGNED_ELSEWHERE, ID_SERVER, S_SHUTDOWN); 
	protocol_copy_header(sbuffer, &SHUTDOWN_HEADER);

	SERVER_PRINT("\nServer: broadcasting S_SHUTDOWN to all clients.\n");
	send_data_to_all(sbuffer, PTCL_HEADER_LENGTH);
}
void Server::Ping::ping_client(struct Client &c) {
	//SERVER_PRINT( "Sending S_PING to client %d. (seq = %d)\n", c.info.id, c.seq_number);
	// THIS IS ASSUMING THE PROTOCOL HEADER HAS BEEN PROPERLY SETUP.
	c.active_ping_seq_number = c.seq_number;
	protocol_update_seq_number(buffer, c.seq_number);
	send_data_to_client(c, buffer, PTCL_HEADER_LENGTH);
}

void Server::Ping::task() {

#define PING_HARD_TIMEOUT_MS 5000
#define TIMEOUT_CHECK_GRANULARITY 2000

	static TIMER ping_timer;
	ping_timer.begin();

	while (is_running()) {
		if (clients.size() < 1) { SLEEP_MS(1000); }
		id_client_map::iterator iter = clients.begin();
		while (iter != clients.end()) {
			struct Client &c = iter->second;
			unsigned milliseconds_passed = c.ping_timer.get_ms();

			if (milliseconds_passed > PING_HARD_TIMEOUT_MS) {
				SERVER_PRINT( "client %u: no C_PING request for more than %d ms (hard timeout), kicking.\n", c.info.id, PING_HARD_TIMEOUT_MS);
				SERVER_PRINT("milliseconds passed: %ld\n", (long)milliseconds_passed);
				iter = remove_client(iter);
			}
			else {
				++iter;
			}

		}
		long wait = TIMEOUT_CHECK_GRANULARITY - ping_timer.get_ms();
		if (wait > 1) { SLEEP_MS(wait); }
		ping_timer.begin();
	}

}

void Server::GameState::task() {
	static TIMER calculate_timer;
	calculate_timer.begin();

	while(is_running()) {
		if (clients.size() <= 0) { SLEEP_MS(500); }
		else {
			for (auto &it : clients) { 
				calculate_state_client(it.second); 
			}
			broadcast_state();
			semi_busy_sleep_until(POSITION_UPDATE_GRANULARITY_MS, calculate_timer);	// for more accuracy
			//PRINT "posupd: %f ms from last\n", calculate_timer.get_ms());
			calculate_timer.begin();
		}
	}
}


void Server::GameState::broadcast_state() {
	PTCLHEADERDATA POSITION_UPDATE_HEADER = 
		protocol_make_header( SEQN_ASSIGNED_ELSEWHERE, ID_SERVER, S_POSITION_UPDATE, clients.size());

	protocol_copy_header(buffer, &POSITION_UPDATE_HEADER);
	size_t total_size = PTCL_HEADER_LENGTH;
	for (auto &it : clients) {
		// construct packet to be distributed
		const struct Client &c = it.second;
		total_size += copy_to_buffer(VAR_SZ(c.info.id), total_size);
		total_size += copy_to_buffer(VAR_SZ(c.car.data_serial), total_size);

	}
	send_data_to_all(buffer, total_size);
}

void Server::GameState::calculate_state_client(struct Client &c) {

	Car &car = c.car;

	float car_acceleration = 0.0;
	float car_prev_velocity = car.state.velocity;

	car.state.velocity *= velocity_dissipation;	// regardless of throttle/brake keystate
	car.data_internal.front_wheel_tmpx *= tmpx_dissipation;

	if (c.keystate & C_KEYSTATE_UP) { car.state.velocity += accel_modifier; }

	if (c.keystate & C_KEYSTATE_DOWN) { car.state.velocity -= brake_modifier; }

	if (c.keystate & C_KEYSTATE_LEFT) {
		car.data_internal.front_wheel_tmpx = MIN(car.data_internal.front_wheel_tmpx+tmpx_modifier, tmpx_limit);
	} 
	if (c.keystate & C_KEYSTATE_RIGHT) {
		car.data_internal.front_wheel_tmpx = MAX(car.data_internal.front_wheel_tmpx-tmpx_modifier, -tmpx_limit);
	}
	if (!(c.keystate & C_KEYSTATE_LEFT) && !(c.keystate & C_KEYSTATE_RIGHT)){
		car.data_internal.front_wheel_tmpx *= 0.45;
		car.state.susp_angle_roll *= 0.89;
	}
	car.state.front_wheel_angle = f_wheel_angle(car.data_internal.front_wheel_tmpx);

	car.state.susp_angle_roll = car.data_internal.front_wheel_tmpx*fabs(car.state.velocity)*0.2;

	float turn_coeff = turn_velocity_coeff(car.state.velocity);
	car.state.direction += (car.state.velocity > 0 ? turning_modifier_forward : turning_modifier_reverse)
		*car.state.front_wheel_angle*car.state.velocity*turn_coeff*POSITION_UPDATE_DT_COEFF;

	car_prev_velocity = 0.5*(car.state.velocity+car_prev_velocity);
	car_acceleration = 0.2*(car.state.velocity - car_prev_velocity) + 0.8*car_acceleration;


	car.state.wheel_rot -= 1.07*car.state.velocity * POSITION_UPDATE_DT_COEFF;
	car.data_internal.susp_angle_fwd = 7*car_acceleration;
	car.state.pos[0] += car.state.velocity*sin(car.state.direction-M_PI/2) * POSITION_UPDATE_DT_COEFF;
	car.state.pos[2] += car.state.velocity*cos(car.state.direction-M_PI/2) * POSITION_UPDATE_DT_COEFF;
}

void Server::increment_client_seq_number(struct Client &c) {
	++c.seq_number;
}

int Server::send_data_to_client(struct Client &client, const char* buffer, size_t data_size) {
	if (data_size > PACKET_SIZE_MAX) {
		SERVER_PRINT( "send_data_to_client: WARNING! data_size > PACKET_SIZE_MAX. Truncating -> errors inbound.\n");
		data_size = PACKET_SIZE_MAX;
	}
	int bytes = socket.send_data(&client.address, buffer, data_size);
	increment_client_seq_number(client);
	return bytes;
}




