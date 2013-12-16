#ifndef NET_SOCKET_H
#define NET_SOCKET_H

#ifdef _WIN32
#include <Winsock2.h>
#include <Ws2tcpip.h>

#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>

#endif

#include <cstring>
#define PACKET_SIZE_MAX 256

#define _OUT

class Socket {

	int fd;
	struct sockaddr_in my_addr;
	unsigned short port;
	bool bad;
	
	int set_blocking_mode(bool blocking);

public:
#ifdef _WIN32
	static int initialized();
	static int initialize();	// winsock requires WSAStartup and all that stuff
	static void deinitialize();
#endif
	
	Socket(unsigned short port, int TYPE, bool blocking);
	int send_data(const struct sockaddr_in *recipient, const char* buffer, size_t len);
	int receive_data(char *output_buffer, struct sockaddr_in _OUT *from);
	int wait_for_incoming_data(int milliseconds);
	
	char get_packet_buffer_char(int index);
	void destroy();
	int get_fd() const { return fd; }
	unsigned short get_port() const { return port; }
	struct sockaddr_in get_own_addr() const { return my_addr; }
	bool is_bad() const { return bad; }

	Socket() { memset(this, 0, sizeof(*this)); }

};

#endif
