#ifndef TIMER_H
#define TIMER_H

#ifdef _WIN32
#include <Windows.h>

#elif __linux__
#include <stdlib.h>
#include <time.h>
#include <string.h>
#endif

class TIMER {

private:
#ifdef _WIN32
	double cpu_freq_hz;	
	__int64 counter_start;

#elif __linux__
	struct timespec beg;

#endif

public:
	void begin();
	double get_ns() const;
	double get_us() const;
	double get_ms() const;
	double get_s() const;

	TIMER() { memset(&beg, 0, sizeof(struct timespec)); }
};

#endif
