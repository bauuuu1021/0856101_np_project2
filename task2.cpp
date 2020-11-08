#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <string.h>
#include <iostream>
#include <arpa/inet.h>
#include <sys/types.h>
#include <errno.h>
#include <sys/time.h>
#include <algorithm>
#include <string>
#include <map>

#define MAX_CLIENTS 30
#define star_divider "****************************************\n"
#define welcome_msg star_divider "** Welcome to the information server. **\n" star_divider

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

map<user_id, client_info> user_table;
fd_set readfds;
int client_fd[MAX_CLIENTS] = {0};

extern void shell(user_id sender, string cmd);
extern void broadcast(string msg);

int socket_setup(int port) {

    struct sockaddr_in address;
    int server_fd, addrlen = sizeof(address); 

    address.sin_family = AF_INET; 
    address.sin_addr.s_addr = INADDR_ANY; 
    address.sin_port = htons(port);

    if (!(server_fd = socket(AF_INET, SOCK_STREAM, 0))) { 
        perror("socket failed"); 
        exit(EXIT_FAILURE); 
    } 

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) { 
        perror("bind failed"); 
        exit(EXIT_FAILURE); 
    } 

    if (listen(server_fd, 30) < 0) { 
        perror("listen"); 
        exit(EXIT_FAILURE); 
    }

    return server_fd;
}

void new_connection(int server) {

    struct sockaddr_in address;
    int addrlen = sizeof(address), new_socket;

    if (FD_ISSET(server, &readfds)) { 
        if ((new_socket = accept(server, (struct sockaddr *)&address, (socklen_t*)&addrlen)) < 0) { 
            perror("accept"); 
            exit(EXIT_FAILURE); 
        } 

        // Send new connection greeting message 
        if (send(new_socket, welcome_msg, strlen(welcome_msg), 0) != strlen(welcome_msg)) { 
            perror("send"); 
        } 

        // Add new socket to array of sockets 
        for (int i = 0; i < MAX_CLIENTS; i++) { 
            if (!client_fd[i]) { // the slot is empty
                client_info tmp;
                tmp.socket_fd = client_fd[i] = new_socket;
                tmp.name = "(no name)"; 
                tmp.delay_pipe_table.clear();
                user_table.insert(pair<user_id, client_info>(i, tmp));

                break; 
            } 
        }

        // Broadcast new connection information  
        string new_conn =   "*** User '(no name)' entered from " + string(inet_ntoa(address.sin_addr)) + \
                             ":" + to_string(ntohs(address.sin_port)) + " ***\n";  
        broadcast(new_conn);
        
        // Send %
        send(new_socket, "% ", 2, 0);
    }
}

void client_query() {

    struct sockaddr_in address;
    int valread, addrlen = sizeof(address);  		
    char buffer[1024];

    for (user_id id = 0; id < MAX_CLIENTS; id++) {

        if (FD_ISSET(client_fd[id], &readfds)) { 

            memset(buffer, 0, 1024);

            // The socket is closed 
            if ((valread = read(client_fd[id], buffer, 1024)) == 0) { 
                map<user_id, client_info>::iterator it = user_table.find(id); 
                string msg = "*** User '" + it->second.name + "' left. ***\n";
                broadcast(msg);
                user_table.erase(it);  
                close(client_fd[id]); 
                client_fd[id] = 0; // Mark as 0 to reuse
            } 

            // Complete client's query 
            else { 
                shell(id, string(buffer));
            } 
        } 
    }
}

int main(int argc , char **argv) {

    if (argc < 2) {
        cerr << "Usage: " << argv[0] << " [port]\n";
        exit(EXIT_FAILURE);
    }

    int activity, max_sd;
    int server_fd = socket_setup(atoi(argv[1]));

    while (true) { 

        FD_ZERO(&readfds);
        FD_SET(server_fd, &readfds); 
        max_sd = server_fd; 

        // Add client sockets into read list
        for (user_id id = 0 ; id < MAX_CLIENTS; id++) {           
            if (client_fd[id] > 0)
                FD_SET(client_fd[id], &readfds);
            max_sd = max(client_fd[id], max_sd); 
        } 

        activity = select(max_sd + 1, &readfds, NULL, NULL, NULL); 
        if ((activity < 0) && (errno != EINTR)) { 
            printf("select error"); 
        } 

        new_connection(server_fd);

        client_query();

    }
} 
