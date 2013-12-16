#include "net/taskthread.h"
#include "net/protocol.h"
#include "text.h"

int copy_to_ext_buffer(char *dst_buffer, const void* src, size_t src_size, size_t buffer_offset) {
	int diff = buffer_offset + src_size - PACKET_SIZE_MAX;
	if (diff >= 0) { 
		PRINT("WARNING! copy_to_buffer: offset + length > PACKET_SIZE_MAX - PTCL_HEADER_LENGTH. errors inbound!\n");
		src_size = diff;
	}
	memcpy(dst_buffer + buffer_offset, src, src_size);
	return src_size;

}

void copy_from_ext_buffer(const char* src_buffer, void* dst, size_t src_size, size_t buffer_offset) {
	memcpy(dst, src_buffer+buffer_offset, src_size);
}


int NetTaskThread::copy_to_buffer(const void* src, size_t src_size, size_t buffer_offset) {
	int diff = buffer_offset + src_size - PACKET_SIZE_MAX;
	if (diff >= 0) { 
		PRINT("WARNING! copy_to_buffer: offset + length > PACKET_SIZE_MAX - PTCL_HEADER_LENGTH. errors inbound!\n");
		src_size = diff;
	}
	memcpy(buffer + buffer_offset, src, src_size);
	return src_size;
}

void NetTaskThread::copy_from_buffer(void* dst, size_t src_size, size_t buffer_offset) {
	memcpy(dst, buffer+buffer_offset, src_size);
}
