#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdbool.h>	
#include <unistd.h>
#include <errno.h>


#define LENGTH 2048
#define NAMELEN 32
#define MAX_CLIENTS 10
#define RED "\x1B[31m"
#define GREEN "\x1B[32m"
#define YELLOW "\x1B[33m"
#define RESET "\033[0m"


static _Atomic unsigned int client_counter = 0;
static int uid = 10;

typedef struct{
	struct sockaddr_in address;
	int socket_fd;
	int uid;
	char name[NAMELEN];
} client_t;

client_t *clients[NAMELEN];

pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;


void join_chat(client_t *cl){
	pthread_mutex_lock(&clients_mutex);

	for(int i=0; i < NAMELEN; ++i){
		if(!clients[i]){
			clients[i] = cl;
			break;
		}
	}

	pthread_mutex_unlock(&clients_mutex);
}


void leave_chat(int uid){
	pthread_mutex_lock(&clients_mutex);

	for(int i=0; i < NAMELEN; ++i){
		if(clients[i]){
			if(clients[i]->uid == uid){
				clients[i] = NULL;
				break;
			}
		}
	}

	pthread_mutex_unlock(&clients_mutex);
}


void send_message(char *s, int uid){
	pthread_mutex_lock(&clients_mutex);

	for(int i=0; i < NAMELEN; ++i){
		if(clients[i]){
			if(clients[i]->uid != uid){
				if(write(clients[i]->socket_fd, s, strlen(s)) < 0){
					perror("Sending Message Failed");
					break;
				}
			}
		}
	}

	pthread_mutex_unlock(&clients_mutex);
}


void *handle_client(void *arg){
	char message[LENGTH];
	char name[NAMELEN];
	int client_connected = 0;
	client_counter++;
	client_t *client = (client_t *)arg;

	if(recv(client->socket_fd, name, NAMELEN, 0) <= 0 || strlen(name) <  2 || strlen(name) >= NAMELEN-1){
		printf("Didn't enter a name.\n");
		client_connected = 1;
	} else{
		strcpy(client->name, name);
		sprintf(message,"%s%s has joined the chat!%s\n", YELLOW, client->name,RESET);
		printf("%s", message);
		send_message(message, client->uid);
	}
	memset(message,0, LENGTH);

	while(true){
		if (client_connected) {
			break;
		}

		int receive = recv(client->socket_fd, message, LENGTH, 0);
		if (receive > 0){
			if(strlen(message) > 0){
				send_message(message, client->uid);
			}
		} else if (receive == 0 || strcmp(message, "exit") == 0){
			sprintf(message,"%s%s has left!%s\n", YELLOW, client->name,RESET);
			printf("%s", message);
			send_message(message, client->uid);
			client_connected = 1;
		} else {
			printf("ERROR: -1\n");
			client_connected = 1;
		}

		memset(message,0, LENGTH);
	}

    close(client->socket_fd);
    leave_chat(client->uid);
    free(client);
    client_counter--;
    pthread_detach(pthread_self());

	return NULL;
}

int main(){

    struct sockaddr_in server_address;
    struct sockaddr_in client_address;
    pthread_t tid;

    int port;
    char term;
    printf("Please Enter The Port Number: ");

    if(scanf("%d%c", &port, &term) != 2 || term != '\n'){
            printf("Not a valid port number. Goodbye.\n");
            return 1;
    }

    int socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = htonl(INADDR_ANY);
    server_address.sin_port = htons(port);

    struct sigaction action;
    action.sa_handler = SIG_IGN;
    sigaction(SIGPIPE, &action, 0);


	if(setsockopt(socket_fd, SOL_SOCKET,(SO_REUSEPORT | SO_REUSEADDR),"1",sizeof(1)) < 0){
		perror("Set Socket Failed");
    	return 1;
	}

    if(bind(socket_fd, (struct sockaddr*)&server_address, sizeof(server_address)) < 0) {
        perror("Socket Binding Failed");
        return 1;
    }

    if (listen(socket_fd, 10) < 0) {
        perror("Socket Listening Failed");
        return 1;
    }

	printf("%sChat Server Running and Listening ------------------%s\n" , YELLOW, RESET);

	while(true){
		socklen_t client_len = sizeof(client_address);
		int connection_fd = accept(socket_fd, (struct sockaddr*)&client_address, &client_len);

		if((client_counter + 1) == MAX_CLIENTS){
			close(connection_fd);
			continue;
		}

		client_t *client = (client_t *)malloc(sizeof(client_t));
		client->address = client_address;
		client->socket_fd = connection_fd;
		client->uid = uid++;

		join_chat(client);
		pthread_create(&tid, NULL, &handle_client, (void*)client);

		sleep(1);
	}

	return 0;
}
