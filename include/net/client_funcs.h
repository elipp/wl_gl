#ifndef CLIENT_FUNCS_H
#define CLIENT_FUNCS_H
#include <unordered_map>
#include <vector>

#include "text.h"
#include "net/client.h"

typedef int (*client_funcptr)(const std::vector<std::string> &);

int help (const std::vector<std::string> &args);
int connect(const std::vector<std::string> &args);
int disconnect(const std::vector<std::string> &args);
int quit(const std::vector<std::string> &args);
int set_name(const std::vector<std::string> &args);
int startserver(const std::vector<std::string> &args);

const std::unordered_map<std::string, const client_funcptr> create_func_map();
extern const std::unordered_map<std::string, const client_funcptr> funcs;

#endif