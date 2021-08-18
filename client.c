#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <signal.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <ctype.h>

#define LENGTH 2048
#define NAMELEN 32
#define RED "\x1B[31m"
#define GREEN "\x1B[32m"
#define YELLOW "\x1B[33m"
#define RESET "\033[0m"

volatile sig_atomic_t client_connected = 0;
int socket_fd = 0;
char name[NAMELEN];

void message_seperator() {
	printf("----------------------------------------\n");
  	printf("%s> %s", YELLOW,RESET);
	fflush(stdout);
}

void format_input (char* arr, int length) {
  int i;
  for (i = 0; i < length; i++) {
    if (arr[i] == '\n') {
      arr[i] = '\0';
      break;
    }
  }
}

char *trim_input(char *str){
    size_t len = strlen(str);
    while(isspace(str[len - 1])){
		if(len == 1){
			return strndup(str, len);	
		}else{
			--len;
		}
    }
    while(*str && isspace(*str)) ++str, --len;
    return strndup(str, len);
}

void catch_exit(int sig) {
    client_connected = 1;
}

void send_message() {
  char message[LENGTH] = {};
  char buffer[LENGTH + (NAMELEN * 2)] = {};

  while(true) {
	message_seperator();
	fgets(message, LENGTH, stdin);
	format_input(message, LENGTH);

	if(strlen(message)>1){
		char *s  = trim_input(message);
		strcpy(message, s);
		free(s);
	}

	if (strcmp(message, "exit") == 0) {
		break;
    }else if(strcmp(message, "") == 0  || strcmp(message, " ") == 0){
        printf("%sEmpty message - Did not sent to other users!\n%s",RED,RESET);
    } else {
      	sprintf(buffer, "%s%s: %s%s\n", GREEN, name,RESET, message);
     	send(socket_fd, buffer, strlen(buffer), 0);
    }
	memset(message, 0, LENGTH);
	memset(buffer, 0,  LENGTH + (NAMELEN * 2));
  }
  catch_exit(2);
}

void receive_message() {
	char message[LENGTH] = {};
  	while (true) {
		int receive = recv(socket_fd, message, LENGTH, 0);
		if (receive > 0) {
			printf("%s", message);
			message_seperator();
		} else if (receive == 0) {
			break;
		} else {
			//do nothing
		}
		memset(message, 0, sizeof(message));
  }
}


int main(){

    char *ip = "127.0.0.1";
    int port;
	char term;
    printf("Please Enter The Port Number: ");

    if(scanf("%d%c", &port, &term) != 2 || term != '\n'){
		printf("Not a valid port number. Goodbye.\n");
		return 1;
    }

	struct sigaction action;
    action.sa_handler = SIG_IGN;
    sigaction(SIGPIPE, &action, 0);

	printf("Please enter your name: ");
  	fgets(name, NAMELEN, stdin);
  	format_input(name, strlen(name));

	if (strlen(name) > NAMELEN || strlen(name) < 2){
		printf("Please Enter Correct Name ( 2 - 30 Characters).\n");
		return 1;
	}

	struct sockaddr_in client_address;
	socket_fd = socket(AF_INET, SOCK_STREAM, 0);
	client_address.sin_family = AF_INET;
	client_address.sin_addr.s_addr = inet_addr(ip);
	client_address.sin_port = htons(port);

	int err = connect(socket_fd, (struct sockaddr *)&client_address, sizeof(client_address));
	if (err == -1) {
		printf("%s Connection Error %s\n",RED, RESET);
		return 1;
	}

	send(socket_fd, name, NAMELEN, 0);

	system("clear");

	printf("%s+++++++ Welcome To Terminal Chat +++++++%s\n", RED ,RESET);

	pthread_t send_message_thread;
  	if(pthread_create(&send_message_thread, NULL, (void *) send_message, NULL) != 0){
		printf("%s Creating Thread Error %s\n",RED, RESET);
    	return 1;
	}

	pthread_t receive_message_thread;
  	if(pthread_create(&receive_message_thread, NULL, (void *) receive_message, NULL) != 0){
		printf("%s Creating Thread Error %s\n ",RED, RESET);
		return 1;
	}

	while (true){
		if(client_connected){
			printf("%s\n Bye %s\n",RED, RESET);
			break;
    	}
	}

	close(socket_fd);

	return 0;
}
