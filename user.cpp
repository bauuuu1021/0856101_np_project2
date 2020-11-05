#include <iostream>
#include <string>
#include <sys/types.h>
#include <sys/socket.h>

#define MAX_CLIENTS 30

using namespace std;

extern int client_fd[MAX_CLIENTS];

void yell(int sender, string msg) {

    string name = "*** " + to_string(sender) + " yelled ***: ";
    for (const auto &c :client_fd) {
        send(c, name.c_str(), name.length(), 0);
        send(c, msg.c_str(), msg.length(), 0);
    }
}
