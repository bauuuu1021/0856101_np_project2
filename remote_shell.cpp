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

extern void shell(int client);

void waitChildHandler(int signo) {
    int status;
    while (waitpid(-1, &status, WNOHANG) > 0);
}

int main(int argc, char **argv) {  
    
    if (argc < 2) {
        cerr << "Usage: ./np_simple [port]\n";
        exit(EXIT_FAILURE);
    }

    signal(SIGCHLD, waitChildHandler);

    // Initial socket settings
    struct sockaddr_in address;
    int server_fd, client_fd, addrlen = sizeof(address); 

    address.sin_family = AF_INET; 
    address.sin_addr.s_addr = INADDR_ANY; 
    address.sin_port = htons(atoi(argv[1]));

    if (!(server_fd = socket(AF_INET, SOCK_STREAM, 0))) { 
        perror("socket failed"); 
        exit(EXIT_FAILURE); 
    } 
 
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) { 
        perror("bind failed"); 
        exit(EXIT_FAILURE); 
    } 

    if (listen(server_fd, 5) < 0) { 
        perror("listen"); 
        exit(EXIT_FAILURE); 
    } 

    if ((client_fd = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen)) < 0) { 
        perror("accept"); 
        exit(EXIT_FAILURE); 
    } 

    //dup2(client_fd, STDIN_FILENO);
    dup2(client_fd, STDOUT_FILENO);
    dup2(client_fd, STDERR_FILENO);

    shell(client_fd);

    close(server_fd);
    close(client_fd);
} 

