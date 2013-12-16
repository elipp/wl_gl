#include "net/client_funcs.h"
#include "net/server.h"

const std::unordered_map<std::string, const client_funcptr> funcs = create_func_map();

int help (const std::vector<std::string> &args) {
	PRINT("List of available commands:\n");
	for (auto &it : funcs) {
		PRINT(" %s\n", it.first.c_str());
	}
	return 1;
}

int connect(const std::vector<std::string> &args) {
	// if properly formatted, we should find ip_port_string at position 1
	if (LocalClient::is_connected()) {
		PRINT("connect: already connected!\n");
		return 0;
	}
	if (args.size() == 1) {
		// use default
		return LocalClient::connect("127.0.0.1:50000");
	}

	else if (args.size() != 2) {
		return 0;
	}
	else {
		return LocalClient::connect(args[1]);
	}
}

int set_name(const std::vector<std::string> &args) {
	if (args.size() <= 1) {
		return 0;
	}
	else {
		LocalClient::set_name(args[1]);
	}
	return 1;
}

int disconnect(const std::vector<std::string> &args) {
	if (LocalClient::is_connected()) {	
		LocalClient::stop();
		PRINT("Disconnected from server.\n");
	} else {
		PRINT("/disconnect: not connected.\n");
	}

	return 1;
}

int quit(const std::vector<std::string> &args) {
	LocalClient::quit();
	// stop_main_loop();
	return 1;
}

int startserver(const std::vector<std::string> &args) {
	if (!Server::is_running()) {
		return Server::init(50000);
	}
	else {
		PRINT("Local server already running on port %u! Use /stopserver to stop.\n", Server::get_port());
		return 0;
	}
}

int stopserver(const std::vector<std::string> &args) {
	if (Server::is_running()) {
		PRINT("Client: stopping local server.\n");
		Server::shutdown();	// should look for a way to do this without blocking the rendering thread
	}
	return 1;
}

// because those c++11 initializer lists aren't (fully?) supported
const std::unordered_map<std::string, const client_funcptr> create_func_map() {
	std::unordered_map<std::string, const client_funcptr> funcs;
	typedef std::pair<std::string, const client_funcptr> str_func_p;

	#define FUNC_INSERT(cmdstr, funcptr) do {\
		funcs.insert(str_func_p(cmdstr, funcptr));\
	} while(0)

	FUNC_INSERT("/connect", connect);
	FUNC_INSERT("/disconnect", disconnect);
	FUNC_INSERT("/name", set_name);
	FUNC_INSERT("/quit", quit);
	FUNC_INSERT("/help", help);
	FUNC_INSERT("/startserver", startserver);
	FUNC_INSERT("/stopserver", stopserver);

	return funcs;
}
