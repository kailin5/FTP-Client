#include <sys/socket.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <string.h>

#define MAX 255

int main(int argc, char *argv[]){
	int sock;
	struct sockaddr_in severAddr;
	struct sockaddr_in clientAddr;
	unsigned int cliAddrLen;
	char recvBuffer[MAX] = "";
	unsigned short severPort;
	int recvMsgSize;
	
	if(argc!=2){
		printf("Usage: %s <TCP PORT>\n", argv[0]);
	}

	severPort = atoi(argv[1]);
	if((sock = socket(PF_INET,SOCK_STREAM,0))<0){
		printf("socket() failed.\n");
		exit(1);
	}
	memset(&severAddr, 0, sizeof(severAddr));
	severAddr.sin_family = AF_INET;
	severAddr.sin_addr.s_addr = htonl(INADDR_ANY);
	severAddr.sin_port = htons(severPort);
	if(bind(sock,(struct sockaddr *) &severAddr, sizeof(severAddr)) < 0){
		printf("bind() failed.\n");
		exit(1);
	}
	listen(sock, 1);

	for( ; ; ){
		 int newsockfd = 0;
		 cliAddrLen = sizeof(clientAddr);
		 if((newsockfd = accept(sock, (struct sockaddr *) &clientAddr, &cliAddrLen)) < 0){//create a new socket
			 printf("accept() failed.\n");
			 exit(1);
		 }

         if (recv(newsockfd, recvBuffer, MAX, 0) < 0) {// receive the file name
			 printf("recv() failed.\n");
			 exit(1);
         }
         printf("**************************************************************\n");
         printf("Accept client %s on TCP port %d.\n",inet_ntoa(clientAddr.sin_addr), clientAddr.sin_port);
         printf("This client request for file name: %s.\n",recvBuffer);
	 int fd = 0;
         if((fd = open(recvBuffer, O_RDWR, 0)) < 0){// Judge and open the file
			 printf("open() failed.\n");
			 exit(1);
         }
         int length = 0;
         if((length = lseek(fd,0,SEEK_END)) < 0){
			 printf("lseek() to END failed.\n");
			 exit(1);
         }
         if(lseek(fd,0,SEEK_SET) < 0){
			 printf("lseek() to SET failed.\n");
			 exit(1);
         }
	 char sendBuffer[length];
         if(read(fd, sendBuffer, length) != length){
			 printf("length: %d",length);
			 printf("sendBuffer: %s", sendBuffer);
			 printf("read() failed.\n");
			 exit(1);
         }
         printf("Bengin sending file...\n");
         if(send(newsockfd, sendBuffer, length, 0) != length){
			 printf("send() failed.\n");
			 exit(1);
         }
         printf("End sending file...\n");
         printf("%d bytes of data have been send.\n",length);
         close(fd);
         close(newsockfd);
    }
}
