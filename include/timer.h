#ifndef TIMER_H
#define TIMER_H

#include <stdlib.h>
#include <time.h>
#include <string.h>

class TIMER {
private:
	struct timespec beg;
public:
	void begin();
	double get_ns() const;
	double get_us() const;
	double get_ms() const;
	double get_s() const;

	TIMER() { memset(&beg, 0, sizeof(struct timespec)); }
};

#endif
