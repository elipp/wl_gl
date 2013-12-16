#include "net/client.h"
#include "common.h"
#include <signal.h>
#include <iostream>

void siginthandler(int signum) {
	if (LocalClient::is_connected()) {
		LocalClient::request_shutdown();
	}
}

int main(int argc, char* argv[]) {

	signal(SIGINT, siginthandler);

	LocalClient::set_name(get_login_username());	
	LocalClient::connect("127.0.0.1:50000");

	while (1) { 
		if (LocalClient::is_shutdown_requested()) {
			LocalClient::quit();
			break;
		}
		std::string chat_input;
		std::getline(std::cin, chat_input); 

		if (chat_input == "quit") {
			LocalClient::request_shutdown();
		} else {
			if (LocalClient::is_connected()) {
				LocalClient::send_chat_message(chat_input);
			}
		}
	}

	return 0;

}
