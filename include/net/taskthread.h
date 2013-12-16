#ifndef TASKTHREAD_H
#define TASKTHREAD_H

#include <thread>
#include "net/socket.h"
#include "timer.h"
#include "common.h"

#define HALF_BUSY_SLEEP_LAGGING_BEHIND 0x0
#define HALF_BUSY_SLEEP_OK 0x1

typedef void (*NTTCALLBACK)(void);

int copy_to_ext_buffer(char *dst_buffer, const void* src, size_t src_size, size_t buffer_offset);
void copy_from_ext_buffer(const char* src_buffer, void* dst, size_t src_size, size_t buffer_offset);

class NetTaskThread {
	
	std::thread thread_handle;
	NTTCALLBACK task_callback;
	int running;
public:
	char buffer[PACKET_SIZE_MAX];

	static void run_task(NetTaskThread *t) {
		t->task();	// the derived objects can be passed as NetTaskThreads, no need to static_cast
	}

	int copy_to_buffer(const void* src, size_t src_size, size_t buffer_offset);
	void copy_from_buffer(void* dst, size_t src_size, size_t buffer_offset);
	
	void start() { 
		running = 1;
		if (task_callback != NULL) {
			thread_handle = std::thread(task_callback); 
		}
		else {
			thread_handle = std::thread(&NetTaskThread::run_task, this);
		}
	}
	void stop() { 
		running = 0; 
		if (thread_handle.joinable()) { thread_handle.join(); }
	}

	int is_running() const { return running; }
	
	NetTaskThread() {};

	NetTaskThread(NTTCALLBACK callback) {
		memset(buffer, 0x0, PACKET_SIZE_MAX);
		task_callback = callback;
		running = 0;
	}

	inline int semi_busy_sleep_until(double ms, const TIMER &timer) {
		if (timer.get_ms() > ms) { return HALF_BUSY_SLEEP_LAGGING_BEHIND; }
		if (ms > 4) { SLEEP_MS((int)ms - 2); } // a ~2 ms timer resolution is implied, which is ideally given by timeBeginPeriod(1)
		while (timer.get_ms() < ms);	// just busy wait those last milliseconds
		return HALF_BUSY_SLEEP_OK;
	}
	
protected:
	virtual void task() = 0;

};


#endif
