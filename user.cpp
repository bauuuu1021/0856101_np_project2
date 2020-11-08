#include <iostream>
#include <string>
#include <sys/types.h>
#include <sys/socket.h>
#include <map>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define MAX_CLIENTS 30

using namespace std;

typedef int user_id;
typedef struct pipe_info {
    int read_fd;
    int write_fd;
} pipe_info;
typedef struct client_info {
    int socket_fd;
    string name;
    long cmd_count;
    map<int, pipe_info> delay_pipe_table;
} client_info;

extern long cmd_count;
extern map<int, pipe_info> delay_pipe_table;
extern int client_fd[MAX_CLIENTS];
extern void printenv(string cmd);
extern void setenv(string cmd);
extern string skip_lead_space(string str);
extern void execute_cmd(string cmd);
extern map<user_id, client_info> user_table;

void broadcast(string msg) {
    for (const auto &c: client_fd)
        if (c)
            send(c, msg.c_str(), msg.length(), 0);
}

void who(int sender_id) {

    cout << "<ID>\t<nickname>\t<IP:port>\t<indicate me>\n" << flush;

    for (std::map<user_id, client_info>::iterator it = user_table.begin(); it != user_table.end(); ++it) {
        struct sockaddr_in address;
        int addrlen = sizeof(address);
        getpeername(it->second.socket_fd, (struct sockaddr*)&address, (socklen_t*)&addrlen);

        cout << it->first << "\t" << it->second.name << "\t" << inet_ntoa(address.sin_addr) \
            << ":" << ntohs(address.sin_port);

        if (sender_id == it->first)
            cout << "\t<- me\n" << flush;
        else
            cout << "\n" << flush; 
    }
}

void yell(int sender_id, string msg) {

    auto user = user_table.find(sender_id)->second;
    msg = msg + "\n";

    string name = "*** " + user.name + " yelled ***: ";
    broadcast(name + msg);
}

void rename(int sender_id, string name) {
    std::map<user_id, client_info>::iterator it = user_table.find(sender_id);
    it->second.name = name = skip_lead_space(name);

    struct sockaddr_in address;
    int addrlen = sizeof(address);
    getpeername(it->second.socket_fd, (struct sockaddr*)&address, (socklen_t*)&addrlen);

    string msg = "*** User from " + string(inet_ntoa(address.sin_addr)) \
                  + ":" + to_string(ntohs(address.sin_port)) + " is named '" + name + "'. ***\n";
    broadcast(msg);
}

void daemon(int sender_id, string cmd) {

    map<user_id, client_info>::iterator it = user_table.find(sender_id);

    for (auto i = 0; i < cmd.size(); i++)
        if (cmd.at(i) == '\r')  // remove carriage return character
            cmd = cmd.substr(0, i);
    cmd = skip_lead_space(cmd);

    // TODO: Check if pipe to other client is needed  
    int old_stdout = dup(STDOUT_FILENO);
    int old_stderr = dup(STDERR_FILENO);
    dup2(client_fd[sender_id], STDOUT_FILENO);
    dup2(client_fd[sender_id], STDERR_FILENO);

    // Commands
    if (!cmd.compare(0, 8, "printenv")) {
        printenv(cmd);
        it->second.cmd_count++;
    }
    else if (!cmd.compare(0, 4, "exit")) {
        return;
    }
    else if (!cmd.compare(0, 6, "setenv")) {
        setenv(cmd);
        it->second.cmd_count++;
    }
    else if (!cmd.compare(0, 3, "who")) {
        who(sender_id);
        it->second.cmd_count++;
    }
    else if (!cmd.compare(0, 4, "yell")) {
        string msg = cmd.substr(5);
        yell(sender_id, msg);
        it->second.cmd_count++;
    }
    else if (!cmd.compare(0, 4, "name")) {
        string name = cmd.substr(5);
        rename(sender_id, name);
        it->second.cmd_count++;
    }
    else if (cmd.length()==0) {     // empty command
        ;
    }
    else {
        cmd_count = it->second.cmd_count;
        delay_pipe_table = it->second.delay_pipe_table;
        
        execute_cmd(cmd);

        it->second.delay_pipe_table = delay_pipe_table;
        it->second.cmd_count++;
    }

    cout << "% " << flush;

    dup2(old_stdout, STDOUT_FILENO);
    dup2(old_stderr, STDERR_FILENO);
}
