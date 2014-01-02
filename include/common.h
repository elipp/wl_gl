#ifndef COMMON_H
#define COMMON_H

#include <algorithm>
#include <vector>
#include <string>
#include <cstdio>
#include <sstream>
#include <fstream>

#define MAX(x, y) std::max((x), (y))
#define MIN(x, y) std::min((x), (y))

#ifdef _WIN32
#define SLEEP_MS(ms) do { Sleep(ms); } while(0)

#define BEGIN_ALIGN16 __declspec(align(16))
#define END_ALIGN16

#elif __linux__
#define SLEEP_MS(ms) do { usleep(1000*ms); } while(0)

#define BEGIN_ALIGN16
#define END_ALIGN16 __attribute__((aligned(16))) 
#endif

#ifndef M_PI
#define M_PI 3.14159265359
#endif

#define PI_PER_TWO (M_PI/2.0)
#define PI_PER_180 (M_PI/180.0)
#define D180_PER_PI (180.0/M_PI);

#define LIMIT_VALUE_BETWEEN(val, lower_limit, upper_limit) MAX((lower_limit), MIN((val), (upper_limit)))

inline float RADIANS(float angle_in_degrees) { return angle_in_degrees*PI_PER_180; }
inline float DEGREES(float angle_in_radians) { return angle_in_radians*D180_PER_PI; }

#ifdef __linux__
#include <unistd.h>
#include <time.h>
#endif 

std::string get_timestamp();
std::vector<std::string> split(const std::string &arg, char delim);
std::string get_login_username();

size_t cpp_getfilesize(std::ifstream& in);

#endif
