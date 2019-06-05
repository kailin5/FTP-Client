#include <stdio.h>
#include <sys/socket.h>
#include <arpa/inet.h> 
#include <stdlib.h>
#include <string.h> 
#include <unistd.h> 
#include <fcntl.h>

#define MAX 255
int main(int argc, char *argv[]) {
	int sock;
	struct sockaddr_in servAddr;
	struct sockaddr_in fromAddr; 
	unsigned short servPort;
	unsigned int fromSize; 
	char *servIP; 
	char *string; 
	char buffer[MAX+1]; 
	int stringLen; 
	int respStringLen; 

	if (argc != 4) {
		printf("Usage: %s <Server IP> <File name> <sever Port>\n", argv[0]);
		exit(1);
	}
	
	servIP = argv[1];
	string = argv[2];
	if ((stringLen = strlen(string)) > MAX)
		printf("File name too long.\n"); 
	servPort = atoi(argv[3]);

	if ((sock = socket(PF_INET, SOCK_STREAM, 0)) < 0)
		printf("socket() failed.\n"); 
	memset(&servAddr, 0, sizeof(servAddr));
	servAddr.sin_family = AF_INET;
	servAddr.sin_addr.s_addr = inet_addr(servIP);
	servAddr.sin_port = htons(servPort);
	if(connect(sock, (struct sockaddr *) &servAddr, sizeof(servAddr)) < 0){
		printf("connect() failed.\n");
		exit(1);
	}
	printf("Connected to the sever: %s.\n",servIP);
	if(send(sock, string, stringLen, 0) != stringLen){
		printf("send() failed.\n");
		exit(1);
	}
	printf("Finished sending request.\n");
	int fd = 0;
	strcat(string,".bak");
	if((fd = open(string, O_RDWR|O_CREAT,0644)) == -1){  //外部可读可写 
		printf("open() failed.\n");
		exit(1);
	}
	int bufferLen = 0;
	if ((respStringLen = recv(sock, buffer, MAX, 0)) < 0) {
		 printf("recv() failed.\n");
		 exit(1);
        }
	printf("Finished receiving file.\n");
	if (write(fd, buffer, respStringLen) != respStringLen) {
		 printf("write() failed.\n");
		 exit(1);
        }
	//printf("buffer: %s\n",buffer);
	printf("%d bytes of data have been received and stored in %s.\n", respStringLen, string);
        close(fd);
        close(sock);	
}
