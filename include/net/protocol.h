#ifndef NET_PROTOCOL_H
#define NET_PROTOCOL_H

#include <cstring>
#include <cstdio>
#include <string>

#ifdef __linux__
#include <arpa/inet.h>
#endif

#include "socket.h"

typedef union {
		unsigned short us;
		unsigned char ch[2];
} command_arg_mask_union;

struct PTCLHEADERDATA {
	int protocol_id;
	unsigned int seq_number;
	unsigned short sender_id;
	command_arg_mask_union cmd_arg_mask;
};


const int PROTOCOL_ID = 0xABACABAD;	// can't use a #define here: memcpy.
const unsigned short ID_SERVER = 0x0000; 
const unsigned short ID_CLIENT_UNASSIGNED = 0xFFFF;

#define PTCL_HEADER_LENGTH (sizeof(PTCLHEADERDATA))

const size_t PTCL_PACKET_BODY_LEN_MAX = PACKET_SIZE_MAX-PTCL_HEADER_LENGTH;

// command bytes

#define S_HANDSHAKE_OK (unsigned char) 0xE1
#define S_POSITION_UPDATE (unsigned char) 0xE2
#define S_PEER_LIST (unsigned char) 0xE3
#define S_CLIENT_CONNECT (unsigned char) 0xE4
#define S_CLIENT_DISCONNECT (unsigned char) 0xE5
#define S_PING (unsigned char) 0xE6
#define S_PONG (unsigned char) 0xE7
#define S_CLIENT_CHAT_MESSAGE (unsigned char) 0xE8
#define S_PACKET_OF_DEATH (unsigned char) 0xEE
#define S_SHUTDOWN (unsigned char) 0xEF

#define C_HANDSHAKE (unsigned char) 0xF1
#define C_KEYSTATE (unsigned char) 0xF2
#define C_PING (unsigned char) 0xF3
#define C_PONG (unsigned char) 0xF4
#define C_CHAT_MESSAGE (unsigned char) 0xF5
#define C_TERMINATE (unsigned char) 0xF6
#define C_QUIT (unsigned char) 0xFF

#define C_KEYSTATE_UP (0x01 << 0)
#define C_KEYSTATE_DOWN (0x01 << 1)
#define C_KEYSTATE_LEFT (0x01 << 2)
#define C_KEYSTATE_RIGHT (0x01 << 3)

#define SEQN_ASSIGNED_ELSEWHERE 0

/* protocol_make_header + protocol_copy_header should probably be wrapped in a single call, since they always go hand in hand */

inline PTCLHEADERDATA protocol_make_header(unsigned int seq_n, unsigned short sender_id, unsigned short cmd_am_us) {
	PTCLHEADERDATA r = { PROTOCOL_ID, seq_n, sender_id, cmd_am_us };
	return r;
}
inline PTCLHEADERDATA protocol_make_header(unsigned int seq_n, unsigned short sender_id, unsigned char cmd_am_ch0, unsigned char cmd_am_ch1) {
	PTCLHEADERDATA r = { PROTOCOL_ID, seq_n, sender_id, cmd_am_ch0 };
	r.cmd_arg_mask.ch[1] = cmd_am_ch1;
	return r;
}

#define VAR_SZ(VARNAME) &VARNAME, sizeof(VARNAME) // to be used with NetTaskThread::copy_from/to_buffer; have had some very elusive bugs because of wrong sizeofs

int protocol_copy_header(char *buffer, const PTCLHEADERDATA *header);
void protocol_get_header_data(const char* buffer, PTCLHEADERDATA *out_data);
void protocol_update_seq_number(char *buffer, unsigned int seq_number);
void buffer_print_raw(const char* buffer, size_t size);

std::string get_dot_notation_ipv4(const struct sockaddr_in *saddr);


#define PTCL_ID_FIELD  (sizeof(int)), 0
#define PTCL_SEQ_NUMBER_FIELD (sizeof(int)), 4
#define PTCL_SENDER_ID_FIELD (sizeof(unsigned short)), 8
#define PTCL_CMD_ARG_FIELD (sizeof(command_arg_mask_union().us)), 10

/*
The following protocol header for all datagram packets is used:

byte offset		content
0-4			(int) PROTOCOL_ID, packages which do not have this identifier will be dropped
4-8			(unsigned int) packet sequence number.
8-10		(unsigned short) sender_id. ID_SERVER for server, ID_CLIENT_UNASSIGNED for unassigned client
10-11			(unsigned char) command byte. see above
11-12			(unsigned char) command byte arg (mostly unused). for example, S_PEER_LIST would have the number of peers here
12-max		<varies by command>

*/

#endif
