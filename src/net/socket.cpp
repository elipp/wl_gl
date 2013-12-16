#include "net/socket.h"
#include "text.h"
#include "common.h"

#ifdef _WIN32
static int socket_initialized = 0;

int Socket::initialized() {
	return socket_initialized;
}

int Socket::initialize() {
	// windows specific; the rest don't need this

	if (initialized()) { return 1; }

	int ret;
	WSAData wsaData;
	ret = WSAStartup(MAKEWORD(2,2), &wsaData);
	
	if (ret != 0) {
		PRINT("WSAStartup failed: %x\n", ret);
		return 0;
	}
	_initialized = 1;
	return 1;
}

void Socket::deinitialize() {
	if (socket_initialized) {
		WSACleanup();
		_initialized = 0; 
	}
}

#endif

int Socket::set_blocking_mode(bool blocking) {
#ifdef _WIN32
	// wsock sockets are blocking by default

	if (!blocking) {
		u_long socket_mode = 1;
		if (ioctlsocket(fd, FIONBIO, &socket_mode) == SOCKET_ERROR) {
			PRINT("Socket: failed to set non-blocking socket\n");
			return 0;
		}
		else {
			PRINT("Socket: using non-blocking socket.\n");
		}
	}
	else { 
		PRINT("Socket: using blocking socket.\n");
	}

	return 1;

#elif __linux__
	if (!blocking) {
		int flags = fcntl(fd, F_GETFL, 0);
		if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0) {
			PRINT("Socket: failed to set non-blocking socket\n");
			return 0;
		}
		else {
			PRINT("Socket: using non-blocking socket.\n");
		}
	}

	return 1;
#endif

}

Socket::Socket(unsigned short port_request, int type, bool blocking) {
	bad = true;

	fd = socket(AF_INET, type, 0);
	
	if (fd < 0) {
		int err;
		#ifdef _WIN32
		err = WSAGetLastError();
		#elif __linux__
		err = errno;
		#endif

		PRINT("Socket: failed to create socket (error code %x).\n", err);
		bad = true;
		return;
	}
	
	my_addr.sin_family = AF_INET;
	my_addr.sin_addr.s_addr = INADDR_ANY;
	my_addr.sin_port = htons(port_request);

	while (bind(fd, (const sockaddr*)&my_addr, sizeof(struct sockaddr_in) ) < 0) {
		PRINT("Socket: bind request at port %u failed, trying port %u next.\n", port_request, port_request+1);
		++port_request;
		my_addr.sin_port = htons(port_request);
	}

	if (!set_blocking_mode(blocking)) { bad = true; return; }

	port = port_request;
	
	bad = false;

}

int Socket::wait_for_incoming_data(int milliseconds) {
	
	fd_set read_fds;
	FD_ZERO(&read_fds);
	FD_SET(fd, &read_fds);

	struct timeval timeout;

	static const int second_in_milliseconds = 1000;	

	timeout.tv_sec = milliseconds/second_in_milliseconds;
	int modulo = milliseconds%second_in_milliseconds;
	timeout.tv_usec = modulo*second_in_milliseconds;
	
	return select(fd + 1, &read_fds, NULL, NULL, &timeout);
	
}


int Socket::send_data(const struct sockaddr_in *recipient, const char* buffer, size_t len) {
	int bytes = sendto(fd, (const char*)buffer, len, 0, (struct sockaddr*)recipient, sizeof(struct sockaddr));
	return bytes;
}

int Socket::receive_data(char *buffer, struct sockaddr_in _OUT *out_from) {
	
	socklen_t from_length = sizeof(struct sockaddr_in);
	memset(out_from, 0x0, from_length);	// probably not necessary

	int bytes = recvfrom(fd, buffer, PACKET_SIZE_MAX, 0, (struct sockaddr*)out_from, &from_length);
	//PRINT "%d bytes of data available. select returned %d\n", bytes, select_r);
	int real_bytes = MAX(bytes, 0);
	real_bytes = MIN(bytes, PACKET_SIZE_MAX-1);
	buffer[real_bytes] = '\0';
	return real_bytes;

}


void Socket::destroy() {
#ifdef _WIN32
	closesocket(fd);
#elif __linux__
	close(fd);
#endif
}

