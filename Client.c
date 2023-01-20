#include <sys/socket.h>
#include <stdio.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <pthread.h>
#include <sys/types.h>
#include <signal.h>
#include <arpa/inet.h>

#define MAX_CLIENTS 100
#define BUFF_SIZE 2048
#define NAME_LEN 32

volatile sig_atomic_t flag = 0;
int sockfd = 0;
char name[NAME_LEN];

void override_str_stdout(){
	printf("\n%s","> ");
	fflush(stdout);
}

void trim_str_lf(char* arr,int len){
	for(int i = 0; i < len;i++){
		if(arr[i] == '\n'){
			arr[i] = '\0';
			break;
		}
	}
}

void catchExit(){
	flag = 1;
}

void recvMess_handler(){
	char message[BUFF_SIZE] = {};
	while(1){
		int receive = recv(sockfd,message,BUFF_SIZE,0);
		
		if(receive > 0){
			printf("%s ",message);
			override_str_stdout();
		} else if(receive == 0){
			break;
		}
		bzero(message,BUFF_SIZE);
	}
}

void sendMess_handler(){
	char buffer[BUFF_SIZE] = {};
	char message[BUFF_SIZE + NAME_LEN] = {};
	
	while(1){
		override_str_stdout();
		fgets(buffer,BUFF_SIZE,stdin);
		trim_str_lf(buffer,BUFF_SIZE);
		
		if(strcmp(buffer,"exit") == 0){
			break;
		}else{
			sprintf(message,"%s: %s\n",name,buffer);
			send(sockfd,message,strlen(message),0);
		}
		bzero(buffer,BUFF_SIZE);
		bzero(message,BUFF_SIZE + NAME_LEN);
	}
	catchExit(2);
}

int main(int argc,char *argv[]){
	if(argc != 3){
		printf("[-]Usage: %s <port>\n",argv[0]);
		return EXIT_FAILURE;
	}
	
	char *ip = argv[0];
	int port = strtol(argv[2],NULL,10);
	
	//signal
	signal(SIGINT,catchExit);
	printf("[+]Enter your name: ");
	fgets(name,NAME_LEN,stdin);
	trim_str_lf(name,strlen(name));
	
	if(strlen(name) > NAME_LEN - 1 || strlen(name) < 2){
		printf("[-]ERROR: Enter name correctly again: \n");
		return EXIT_FAILURE;
	}
	
	struct sockaddr_in servAddr;
	//configuracion de sockets
	sockfd = socket(AF_INET,SOCK_STREAM,0);
	servAddr.sin_family = AF_INET;
	servAddr.sin_addr.s_addr = inet_addr(ip);
	servAddr.sin_port = htons(port);
	inet_aton(argv[1],&servAddr.sin_addr);
	
	//Conectar al servidor
	int err = connect(sockfd,(struct sockaddr*)&servAddr,sizeof(servAddr));
	if(err == -1){
		printf("[-]ERROR: connection\n");
		return EXIT_FAILURE;
	}
	
	//mandar nombre
	send(sockfd,name,NAME_LEN,0);
	
	//si todo == success, server escucha
	printf("%s===***=== WELCOME TO THE CHATROOM : CLIENT ===***===%s\n",name,name);
	
	pthread_t sendMess;
	pthread_t recvMess;
	if(pthread_create(&sendMess,NULL,(void*)sendMess_handler,NULL) != 0){
		printf("[-]ERROR: pthread Send\n");
		return EXIT_FAILURE;
	}
	if(pthread_create(&recvMess,NULL,(void*)recvMess_handler,NULL) != 0){
		printf("[-]ERROR: pthread Receive\n");
		return EXIT_FAILURE;
	}
	
	while(1){
		if(flag){
			printf("\nYOU'VE LOGGED OUT\n");
			break;
		}
	}
	
	close(sockfd);
	
	return EXIT_SUCCESS;
}