#include <iostream>
#include <string>
#include <sys/types.h>
#include <sys/socket.h>
#include <map>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>

#define MAX_CLIENTS 30

using namespace std;

typedef int user_id;
typedef struct pipe_info {
    int read_fd;
    int write_fd;
} pipe_info;
typedef struct inbox_info {
    int pipe_read;
    string cmd;
} inbox_info;
typedef struct client_info {
    int socket_fd;
    string name;
    long cmd_count;
    map<int, pipe_info> delay_pipe_table;
    map<user_id, inbox_info> inbox;
} client_info;

extern int null_fd;
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

void who(user_id sender_id) {

    cout << "<ID>\t<nickname>\t<IP:port>\t<indicate me>\n" << flush;

    for (std::map<user_id, client_info>::iterator it = user_table.begin(); it != user_table.end(); ++it) {
        struct sockaddr_in address;
        int addrlen = sizeof(address);

        memset(&address, 0, sizeof(address));
        getpeername(it->second.socket_fd, (struct sockaddr*)&address, (socklen_t*)&addrlen);

        cout << it->first << "\t" << it->second.name << "\t" << inet_ntoa(address.sin_addr) \
            << ":" << ntohs(address.sin_port);

        if (sender_id == it->first)
            cout << "\t<- me\n" << flush;
        else
            cout << "\n" << flush; 
    }
}

void yell(user_id sender_id, string msg) {

    auto user = user_table.find(sender_id)->second;
    msg = msg + "\n";

    string name = "*** " + user.name + " yelled ***: ";
    broadcast(name + msg);
}

void tell(user_id sender_id, user_id receiver_id, string msg) {

    if (user_table.find(receiver_id) == user_table.end()) {
        string err = "*** Error: user #" + to_string(receiver_id) + " does not exist yet. ***\n";
        cerr << err << flush;
        return;
    }

    string prefix = "*** " + user_table.find(sender_id)->second.name + " told you ***: ";
    msg = prefix + msg + "\n";
    send(user_table.find(receiver_id)->second.socket_fd, msg.c_str(), msg.length(), 0);
}

void rename(user_id sender_id, string name) {
    std::map<user_id, client_info>::iterator it = user_table.find(sender_id);
    it->second.name = name = skip_lead_space(name);

    struct sockaddr_in address;
    int addrlen = sizeof(address);
    getpeername(it->second.socket_fd, (struct sockaddr*)&address, (socklen_t*)&addrlen);

    string msg = "*** User from " + string(inet_ntoa(address.sin_addr)) \
                  + ":" + to_string(ntohs(address.sin_port)) + " is named '" + name + "'. ***\n";
    broadcast(msg);
}

string pipein_handler(string cmd, map<user_id, client_info>::iterator receiver) {
    int pos;
    string space_delimiter = " ", ret = string(cmd);

    if ((pos = cmd.find("<")) != string::npos && cmd.compare(pos + 1, 1, " ")) {
        ret = cmd.substr(0, pos);

        string sd = cmd.substr(pos + 1);
        pos = sd.find(space_delimiter);
        sd = sd.substr(0, pos);
        user_id sender_id = stoi(sd);

        map<user_id, client_info>::iterator send = user_table.find(sender_id);
        if (send != user_table.end()) {
            if (receiver->second.inbox.find(sender_id) == receiver->second.inbox.end()) {
                dup2(null_fd, STDIN_FILENO);
                cerr << "*** Error: the pipe #<" + to_string(sender_id) + ">->#<" + 
                    to_string(receiver->first) + "> does not exist yet. ***\n";
            }
            else {
                dup2(receiver->second.inbox.find(sender_id)->second.pipe_read, STDIN_FILENO);
                close(receiver->second.inbox.find(sender_id)->second.pipe_read);


                string msg = "*** <" + send->second.name + "> (#<" + to_string(send->first) + 
                    ">) just received from <" + receiver->second.name + 
                    "> (#<" + to_string(receiver->first) + ">) by '" + 
                    receiver->second.inbox.find(sender_id)->second.cmd + "' ***\n";
                broadcast(msg);
            }
        }
        else {
            dup2(null_fd, STDIN_FILENO);
            cerr << "*** Error: user #<" + to_string(sender_id) + "> does not exist yet. ***\n";
        }
    }

    return ret;
}

string pipeout_handler(string cmd, map<user_id, client_info>::iterator sender) {
    int pos;
    string space_delimiter = " ", ret = string(cmd);

    if ((pos = cmd.find(">")) != string::npos && cmd.compare(pos + 1, 1, " ")) {
        ret = cmd.substr(0, pos);

        string rv = cmd.substr(pos + 1);
        pos = rv.find(space_delimiter);
        rv = rv.substr(0, pos);
        user_id receiver_id = stoi(rv);

        map<user_id, client_info>::iterator recv = user_table.find(receiver_id);
        if (recv != user_table.end()) {
            inbox_info new_msg;
            int userpipe[2];
            pipe(userpipe);
            dup2(userpipe[1], STDOUT_FILENO);
            close(userpipe[1]);
            new_msg.cmd = cmd;
            new_msg.pipe_read = userpipe[0];
            recv->second.inbox.insert(pair<user_id, inbox_info>(sender->first, new_msg));

            string msg = "*** " + sender->second.name + " (#" + to_string(sender->first) +
                ") just piped '" + cmd + "' to " + recv->second.name + 
                " (#" + to_string(recv->first) +") ***\n";
            broadcast(msg);
        }
        else {
            dup2(null_fd, STDOUT_FILENO);
            cerr << "*** Error: user #<" + to_string(receiver_id) + "> does not exist yet. ***\n";   
        }
    }

    return ret;
}

void shell(user_id sender_id, string cmd) {

    map<user_id, client_info>::iterator it = user_table.find(sender_id);

    for (auto i = 0; i < cmd.size(); i++)
        if (cmd.at(i) == '\r')  // remove carriage return character
            cmd = cmd.substr(0, i);
    cmd = skip_lead_space(cmd);

    // TODO: Check if pipe to other client is needed  
    int old_stdin = dup(STDIN_FILENO);
    int old_stdout = dup(STDOUT_FILENO);
    int old_stderr = dup(STDERR_FILENO);

    dup2(client_fd[sender_id], STDOUT_FILENO);
    dup2(client_fd[sender_id], STDERR_FILENO);

    cmd = pipein_handler(cmd, it);
    cmd = pipeout_handler(cmd, it);

    // Commands
    if (!cmd.compare(0, 8, "printenv")) {
        printenv(cmd);
        it->second.cmd_count++;
    }
    else if (!cmd.compare(0, 4, "exit")) { 
        string msg = "*** User '" + it->second.name + "' left. ***\n";
        broadcast(msg);
        close(it->second.socket_fd);
        user_table.erase(it); 
        client_fd[sender_id] = 0; // Mark as 0 to reuse

        // TODO: check if needed
        dup2(old_stdin, STDIN_FILENO);
        dup2(old_stdout, STDOUT_FILENO);
        dup2(old_stderr, STDERR_FILENO);

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
    else if (!cmd.compare(0, 4, "tell")) {
        string delimiter = " ", msg = skip_lead_space(cmd.substr(5));
        int pos = msg.find(delimiter);
        int receiver_id = stoi(msg.substr(0, pos));
        tell(sender_id, receiver_id, msg.substr(pos + 1));
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

    dup2(old_stdin, STDIN_FILENO);
    dup2(old_stdout, STDOUT_FILENO);
    dup2(old_stderr, STDERR_FILENO);
}
