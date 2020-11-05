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

#define MAX_CLIENTS 30

using namespace std;

extern void shell();

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

int main(int argc , char **argv) {

    if (argc < 2) {
        cerr << "Usage: " << argv[0] << " [port]\n";
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in address;
	int new_socket , client_fd[MAX_CLIENTS] = {0}, 
		activity, valread , sd, max_sd, addrlen = sizeof(address);  		
	char buffer[1024];
	fd_set readfds; 
		
	//a message 
	char *message = "ECHO Daemon v1.0 \r\n"; 
		
    int server_fd = socket_setup(atoi(argv[1]));
		
	while (true) { 
		FD_ZERO(&readfds);
		FD_SET(server_fd, &readfds); 
		max_sd = server_fd; 
			
		//add child sockets to set 
		for (int i = 0 ; i < MAX_CLIENTS; i++) { 
			//socket descriptor 
			sd = client_fd[i]; 
				
			//if valid socket descriptor then add to read list 
			if (sd > 0) 
				FD_SET( sd , &readfds); 
				
			//highest file descriptor number, need it for the select function 
			if (sd > max_sd) 
				max_sd = sd; 
		} 
	
		//wait for an activity on one of the sockets , timeout is NULL , 
		//so wait indefinitely 
		activity = select(max_sd + 1, &readfds, NULL, NULL, NULL); 
	
		if ((activity < 0) && (errno != EINTR)) { 
			printf("select error"); 
		} 
			
		//If something happened on the master socket , 
		//then its an incoming connection 
		if (FD_ISSET(server_fd, &readfds)) { 
			if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen)) < 0) { 
				perror("accept"); 
				exit(EXIT_FAILURE); 
			} 
			
			//inform user of socket number - used in send and receive commands 
			printf("New connection , socket fd is %d , ip is : %s , port : %d\n" , new_socket , inet_ntoa(address.sin_addr) , ntohs 
				(address.sin_port)); 
		
			//send new connection greeting message 
			if (send(new_socket, message, strlen(message), 0) != strlen(message)) { 
				perror("send"); 
			} 
				
			puts("Welcome message sent successfully"); 
				
			//add new socket to array of sockets 
			for (int i = 0; i < MAX_CLIENTS; i++) { 
				//if position is empty 
				if (!client_fd[i]) { 
					client_fd[i] = new_socket; 
					printf("Adding to list of sockets as %d\n", i); 
						
					break; 
				} 
			} 
		} 
			
		//else its some IO operation on some other socket 
		for (int i = 0; i < MAX_CLIENTS; i++) { 
			sd = client_fd[i]; 
				
			if (FD_ISSET(sd, &readfds)) { 
				//Check if it was for closing , and also read the 
				//incoming message 
				if ((valread = read( sd , buffer, 1024)) == 0) { 
					//Somebody disconnected , get his details and print 
					getpeername(sd, (struct sockaddr*)&address, (socklen_t*)&addrlen); 
					printf("Host disconnected , ip %s , port %d \n" , 
						inet_ntoa(address.sin_addr), ntohs(address.sin_port)); 
						
					//Close the socket and mark as 0 in list for reuse 
					close(sd); 
					client_fd[i] = 0; 
				} 
					
				//Echo back the message that came in 
				else { 
					//set the string terminating NULL byte on the end 
					//of the data read 
					buffer[valread] = '\0'; 
					send(sd, buffer, strlen(buffer), 0); 
				} 
			} 
		} 
	}
} 
