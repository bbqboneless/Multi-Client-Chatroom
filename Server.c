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
#define BUFFER_SIZE 2048 //doble
#define NAME_LEN 32

//estructura del cliente
typedef struct{
	struct sockaddr_in address;
	int sockfd;
	int uid; //user id
	char name[NAME_LEN];
} client_t;

client_t *clients[MAX_CLIENTS];
pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER; //transferir mensajes

//global variables
static _Atomic unsigned int ccont = 0; //client cont
static int uid = 10;

//funciones

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

void intoQueue(client_t *cl){
	pthread_mutex_lock(&clients_mutex);
	
	for(int i = 0;i < MAX_CLIENTS;i++){
		if(!clients[i]){
			clients[i] = cl;
			break;
		}
	}
	
	pthread_mutex_unlock(&clients_mutex);
}

void offQueue(int uid){
	pthread_mutex_lock(&clients_mutex);
	
	for(int i = 0;i < MAX_CLIENTS;i++){
		if(clients[i]){
			if(clients[i]->uid == uid){
				clients[i] = NULL;
				break;
			}
		}
	}
	
	pthread_mutex_unlock(&clients_mutex);
}

//send message to everyone except the sender
void sendMessage(char* s,int uid){
	pthread_mutex_lock(&clients_mutex);
	
	for(int i = 0;i < MAX_CLIENTS;i++){
		if(clients[i]){
			if(clients[i]->uid != uid){
				if(write(clients[i]->sockfd,s,strlen(s)) < 0){
					printf("[-]ERROR: write to descriptor failed\n");
					break;
				}
			}
		}
	}
	
	pthread_mutex_unlock(&clients_mutex);
}

void *clientHandler(void *arg){
	char buffer[BUFFER_SIZE];
	char name[NAME_LEN];
	int lflag = 0; //leave flag
	ccont++;
	
	client_t *cli = (client_t*)arg;
	
	//nombre del cliente
	if(recv(cli->sockfd,name,NAME_LEN,0) <= 0 || strlen(name) < 2 || strlen(name) >= NAME_LEN - 1){
		printf("[-]Enter name correctly\n");
		lflag = 1;
	}else{
		//el cliente se unio al servidor
		strcpy(cli->name,name);
		sprintf(buffer,"[+]%s has joined the chatroom\n",cli->name);
		printf("%s",buffer);
		sendMessage(buffer,cli->uid);
	}
	
	bzero(buffer,BUFFER_SIZE);
	
	while(1){
		if(lflag){
			break;
		}
		
		int receive = recv(cli->sockfd,buffer,BUFFER_SIZE,0);
		
		if(receive > 0){
			if(strlen(buffer) > 0){
				sendMessage(buffer,cli->uid);
				trim_str_lf(buffer,strlen(buffer));
				printf("%s1\n",buffer);
			}
		}else if(receive == 0 || strcmp(buffer,":exit") == 0){
			sprintf(buffer,"[+]%s has left the chatroom\n",cli->name);
			printf("%s",buffer);
			sendMessage(buffer,cli->uid);
			lflag = 1;
		}else{
			printf("[-]ERROR: -1\n");
			lflag = 1;
		}
		
		bzero(buffer,BUFFER_SIZE);
	}
	close(cli->sockfd);
	offQueue(cli->uid);
	free(cli);
	ccont--;
	pthread_detach(pthread_self());
	
	return NULL;
}

int main(int argc,char *argv[]){
	char *ip = argv[0];
	int port = strtol(argv[2],NULL,10),option = 1,listenfd = 0,connfd = 0;
	struct sockaddr_in servAddr;
	struct sockaddr_in cliAddr;
	pthread_t tid;
	
	if(argc != 3){
		printf("[-]Usage: %s <port>\n",argv[0]);
		return EXIT_FAILURE;
	}
	
	//configuracion de sockets
	listenfd = socket(AF_INET,SOCK_STREAM,0);
	servAddr.sin_family = AF_INET;
	servAddr.sin_addr.s_addr = inet_addr(ip);
	servAddr.sin_port = htons(port);
	inet_aton(argv[1],&servAddr.sin_addr); 
	
	//señales
	signal(SIGPIPE,SIG_IGN);
	if(setsockopt(listenfd,SOL_SOCKET,(SO_REUSEPORT | SO_REUSEADDR),(char*)&option,sizeof(option)) < 0){
		printf("[-]ERROR: set socket option\n");
		return EXIT_FAILURE;
	}
	
	//binding
	if(bind(listenfd,(struct sockaddr*)&servAddr,sizeof(servAddr)) < 0){
		printf("[-]ERROR: binding\n");
		return EXIT_FAILURE;
	}
	
	//si bind == success, listen
	if(listen(listenfd,10) < 0){
		printf("[-]ERROR: listening\n");
		return EXIT_FAILURE;
	}
	
	//si todo == success, server escucha
	printf("sammy===***=== WELCOME TO THE CHATROOM : SERVER ===***===sammy\n");
	
	//loop infinito para conectar clientes
	while(1){
		socklen_t clilen = sizeof(cliAddr);
		connfd = accept(listenfd,(struct sockaddr*)&cliAddr,&clilen);
		
		//checamos los clientes maximos
		if((ccont +1) == MAX_CLIENTS){
			printf("[-]MAXIMUM amount of clients reached. Connection Rejected: %s\n",inet_ntoa(servAddr.sin_addr));
			close(connfd);
			continue;
		}
		
		//configuracion de cliente
		client_t *cli = (client_t *)malloc(sizeof(client_t));
		cli->address = cliAddr;
		cli->sockfd = connfd;
		cli->uid = uid++;
		
		//añadir cliente al queue
		intoQueue(cli);
		pthread_create(&tid,NULL,&clientHandler,(void*)cli);
		
		//reducir el uso de CPU
		sleep(1);
	}
	
	return EXIT_SUCCESS;	
}