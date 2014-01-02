#include "net/protocol.h"

#include "net/socket.h"
#include "net/server.h"
#include "text.h"
#include "common.h"

#define IPBUFSIZE 32
std::string get_dot_notation_ipv4(const struct sockaddr_in *saddr) {
	char ip_buf[IPBUFSIZE];
	inet_ntop(saddr->sin_family, &saddr->sin_addr, ip_buf, IPBUFSIZE);
	ip_buf[IPBUFSIZE-1] = '\0';
	return std::string(ip_buf);
}

int protocol_copy_header(char *buffer, const PTCLHEADERDATA *header) {
	memcpy(buffer, header, sizeof(PTCLHEADERDATA));
	return sizeof(PTCLHEADERDATA);
}

void protocol_update_seq_number(char *buffer, unsigned int seq_number) {
	memcpy(buffer+4, &seq_number, sizeof(seq_number)); 
}

void protocol_get_header_data(const char* buffer, _OUT PTCLHEADERDATA *out_data) {
	memcpy(out_data, buffer, sizeof(PTCLHEADERDATA));
}

void buffer_print_raw(const char* buffer, size_t size) {

	PRINT("buffer_print_raw: printing %ld bytes of data.\n", (long)size);
	for (size_t i = 0; i < size; ++i) {
		if ((unsigned char)buffer[i] < 0x7F && (unsigned char)buffer[i] > 0x1F) {
			PRINT("%c  ", buffer[i]);
		}
		else {
			PRINT("%02x ", (unsigned char)buffer[i]);
		}
		if (i % 16 == 15) { PRINT("\n"); }
	}
	PRINT("\n");
}
