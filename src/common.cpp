#include "common.h"

std::string get_timestamp() {
	char buffer[128];
#ifdef _WIN32
	SYSTEMTIME st;
	GetLocalTime(&st);
	sprintf_s(buffer, sizeof(buffer), "%02d:%02d", st.wHour, st.wMinute);

#elif __linux__
	const time_t cur_time = time(NULL);
	struct tm *t = localtime(&cur_time);
	snprintf(buffer, sizeof(buffer), "%02d:%02d", t->tm_hour, t->tm_min);
#endif

	return std::string(buffer);
}

std::vector<std::string> split(const std::string &arg, char delim) {

    std::vector<std::string> elems;

    std::stringstream ss(arg);
    std::string item;

    while (std::getline(ss, item, delim)) {
        elems.push_back(item);
    }

    return elems;
}

std::string get_login_username() {

	#ifdef _WIN32
	char name_buf[UNLEN+1];
	DWORD len = UNLEN+1;
	GetUserName(name_buf, &len);

	#elif __linux__
	char name_buf[L_cuserid];
	int r = getlogin_r(name_buf, L_cuserid);
	if (r != 0) {
		sprintf(name_buf, "Player");
	}
	#endif

	return std::string(name_buf);

}

size_t cpp_getfilesize(std::ifstream& in) {
	in.seekg (0, std::ios::end);
	long length = in.tellg();
	in.seekg(0, std::ios::beg);

	return length;
}

