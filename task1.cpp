#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <string.h>
#include <iostream>

using namespace std;

extern void shell();

void waitChildHandler(int signo) {
    int status;
    while (waitpid(-1, &status, WNOHANG) > 0);
}

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

    int opt;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, 
                                                  &opt, sizeof(opt))) { 
        perror("setsockopt"); 
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

int main(int argc, char **argv) {  

    if (argc < 2) {
        cerr << "Usage: " << argv[0] << " [port]\n";
        exit(EXIT_FAILURE);
    }

    signal(SIGCHLD, waitChildHandler);

    // Initial socket settings
    struct sockaddr_in address;
    int server_fd, client_fd, addrlen = sizeof(address); 

    server_fd = socket_setup(atoi(argv[1]));

    while (true) {
        if ((client_fd = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen)) < 0) { 
            perror("accept"); 
            exit(EXIT_FAILURE); 
        }

        int std_fileno[3] = {STDIN_FILENO, STDOUT_FILENO, STDERR_FILENO}; 
        int old_fd[3] = {0};

        for (int i = 0; i < 3; i++) {
            old_fd[i] = dup(std_fileno[i]);
            dup2(client_fd, std_fileno[i]);
        }
        close(client_fd);

        shell();

        for (int i = 0; i < 3; i++) {
            dup2(old_fd[i], std_fileno[i]);
            close(old_fd[i]);
        }
    }
    close(server_fd);
} 

