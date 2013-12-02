#include "timer.h"

#include <time.h>

void TIMER::begin() {
	clock_gettime(CLOCK_REALTIME, &this->beg);
}

double TIMER::get_ns() const {
	struct timespec end;
	clock_gettime(CLOCK_REALTIME, &end);
	return (end.tv_sec*1000000000 + end.tv_nsec) - (beg.tv_sec*1000000000 + beg.tv_nsec);
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

