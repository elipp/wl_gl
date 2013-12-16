#include "timer.h"

void TIMER::begin() {
#ifdef _WIN32
	LARGE_INTEGER li;

	QueryPerformanceFrequency(&li);
	cpu_freq_hz = double(li.QuadPart);

	QueryPerformanceCounter(&li);
	counter_start = li.QuadPart;

#elif __linux__
	clock_gettime(CLOCK_REALTIME, &this->beg);
#endif

}

double TIMER::get_ns() const {
#ifdef _WIN32
	LARGE_INTEGER tick_count;
	QueryPerformanceCounter(&tick_count);
	return (double)(1000000000*(tick_count.QuadPart-counter_start))/TIMER::cpu_freq_hz;

#elif __linux__
	struct timespec end;
	clock_gettime(CLOCK_REALTIME, &end);
	return (end.tv_sec*1000000000 + end.tv_nsec) - (beg.tv_sec*1000000000 + beg.tv_nsec);

#endif
}

double TIMER::get_us() const {
	return get_ns()/1000.0;
}

double TIMER::get_ms() const {
	return get_ns()/1000000.0;
}

double TIMER::get_s() const {
	return (double)get_ns()/1000000000.0;
}

