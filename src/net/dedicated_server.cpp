#include "net/server.h"
#include "common.h"


#ifdef _WIN32
#include <tchar.h>

BOOL CtrlHandler( DWORD fdwCtrlType ) {
	switch(fdwCtrlType) {
		case CTRL_CLOSE_EVENT:
			Server::shutdown();
			return TRUE;
			break;
		default:
			return FALSE;
			;
	}
}

#elif __linux__

#include <stdio.h>
#include <signal.h>

void sigint_handler(int signum) {

	// to avoid multiple calls to Server::shutdown()
	static int shutting_down = 0;

	if (shutting_down == 0) {
		shutting_down = 1;
		printf("dedicated_server: caught SIGINT, stopping server and exiting.\n");
		Server::shutdown();
		exit(signum);
	}
}


#endif

#ifdef _WIN32
int _tmain(int argc, _TCHAR* argv[]) {

	timeBeginPeriod(1);
	if(!Server::init((unsigned short)50000)) { return EXIT_FAILURE; }        // start server thread
	SetConsoleCtrlHandler( (PHANDLER_ROUTINE) CtrlHandler, TRUE );
	timeEndPeriod(1);

	while (1) SLEEP_MS(2500);	// idle main thread

	return 0;

}

#elif __linux__
int main(int argc, char* argv[]) {

/*	std::string name = get_login_username();
	std::string timestamp = get_timestamp();

	fprintf(stderr, "timestamp: %s, login username: %s\n", timestamp.c_str(), name.c_str()); */
	
	signal(SIGINT, sigint_handler);
	if(!Server::init((unsigned short)50000)) { return EXIT_FAILURE; }        // start server thread

	while (Server::is_running()) SLEEP_MS(2500);

	return 0;

}

#endif
