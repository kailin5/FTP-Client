/* header files */
#include	<stdio.h>
#include	<stdlib.h>
#include	<netdb.h>	/* getservbyname(), gethostbyname() */
#include	<errno.h>	/* for definition of errno */
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/fcntl.h>

/* define macros*/
#define MAXBUF	1024
#define STDIN_FILENO	0
#define STDOUT_FILENO	1

/* define FTP reply code */
#define USERNAME	220
#define PASSWORD	331
#define LOGIN		230
#define PATHNAME	257
#define CLOSEDATA	226
#define ACTIONOK	250

/* DefinE global variables */
char	*host;		/* hostname or dotted-decimal string */
char	*port;
char	*rbuf, *rbuf1;		/* pointer that is malloc'ed */
char	*wbuf, *wbuf1;		/* pointer that is malloc'ed */
struct sockaddr_in	servaddr;

int	cliopen(char *host, char *port);
void	strtosrv(char *str, char *host, char *port);
void	cmd_tcp(int sockfd);
void	ftp_list(int sockfd);
int	ftp_get(int sck, char *pDownloadFileName_s);
int	ftp_put (int sck, char *pUploadFileName_s);

int
main(int argc, char *argv[])
{
	int	fd;

	if (0 != argc-2)
	{
		printf("%s\n","missing <hostname>");
		exit(0);
	}

	host = argv[1];
	// test case: 123.206.33.80 	ilinkai.cn

	port = "21";

	/*****************************************************************
	//1. code here: Allocate the read and write buffers before open().
	// memory allocation for the read and writer buffers for maximum size
	*****************************************************************/
	rbuf = (char *)malloc(MAXBUF*sizeof(char));
    rbuf1 = (char *)malloc(MAXBUF*sizeof(char));
    wbuf = (char *)malloc(MAXBUF*sizeof(char));
    wbuf1 = (char *)malloc(MAXBUF*sizeof(char));


	fd = cliopen(host, port);

	cmd_tcp(fd);

	exit(0);
}


/* Establish a TCP connection from client to server */
int
cliopen(char *host, char *port)
{
	int control_sock;
    //1.FTP 自己的传输地址结构体
    struct hostent *ht = NULL;

     //2.创建套接字
    control_sock = socket(AF_INET,SOCK_STREAM,0);
    if(control_sock < 0)
    {
       printf("socket error\n");
       return -1;
    }

     //3.将IP地址进行转换，变为FTP地址结构体
    ht = gethostbyname(host);
    if(!ht)
    { 
        return -1;
    }
    //Connect to the server and return the control socket
    memset(&servaddr,0,sizeof(struct sockaddr_in));
    memcpy(&servaddr.sin_addr.s_addr,ht->h_addr,ht->h_length);
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(port);

    if(connect(control_sock,(struct sockaddr*)&servaddr,sizeof(struct sockaddr)) == -1)
    {
        return -1;
    }

    return control_sock;
}

/*
   Compute server's port by a pair of integers and store it in char *port
   Get server's IP address and store it in char *host
*/
void
strtosrv(char *str, char *host, char *port)
{
	/*************************************************************
	//3. code here to compute the port number for data connection
	EG:10,3,255,85,192,181  192*256+181 = 49333
	*************************************************************/

	int addr[6];
	printf("%s\n",str);
	sscanf(str,"%*[^(](%d,%d,%d,%d,%d,%d)",&addr[0],&addr[1],&addr[2],&addr[3],&addr[4],&addr[5]);
	bzero(host,strlen(host));
	sprintf(host,"%d.%d.%d.%d",addr[0],addr[1],addr[2],addr[3]);
	int port = addr[4]*256 + addr[5];
    return port;
}


/* Read and write as command connection */
void
cmd_tcp(int sockfd)
{
	int		maxfdp1, nread, nwrite, fd, replycode;
	char		host[16];
	char		port[6];
	fd_set		rset;

	FD_ZERO(&rset);
	maxfdp1 = sockfd + 1;	/* check descriptors [0..sockfd] */

	for ( ; ; )
	{
		FD_SET(STDIN_FILENO, &rset);
		FD_SET(sockfd, &rset);

		if (select(maxfdp1, &rset, NULL, NULL, NULL) < 0)
			printf("select error\n");
			
		/* data to read on stdin */
		if (FD_ISSET(STDIN_FILENO, &rset)) {
			if ( (nread = read(STDIN_FILENO, rbuf, MAXBUF)) < 0)
				printf("read error from stdin\n");
			nwrite = nread+5;

			/* send username */
			if (replycode == USERNAME) {
				sprintf(wbuf, "USER %s", rbuf);
				if (write(sockfd, wbuf, nwrite) != nwrite)
					printf("write error\n");
			}

			 /*************************************************************
			 //4. code here: send password
			 *************************************************************/
			if(replycode == PASSWORD)
		    {
                //printf("%s\n",rbuf1);
                sprintf(wbuf,"PASS %s",rbuf1);
                if(write(sockfd,wbuf,nwrite) != nwrite)
            	    printf("write error\n");
                //bzero(rbuf,sizeof(rbuf));
                //printf("%s\n",wbuf);
                //printf("2:%s\n",wbuf);
             }

			 /* send command */
			if (replycode==LOGIN || replycode==CLOSEDATA || replycode==PATHNAME || replycode==ACTIONOK)
			{
				/* ls - list files and directories*/
				if (strncmp(rbuf, "ls", 2) == 0) {
					sprintf(wbuf, "%s", "PASV\n");
					write(sockfd, wbuf, 5);
					sprintf(wbuf1, "%s", "LIST -al\n");
					nwrite = 9;
					continue;
				}

				/*************************************************************
				// 5. code here: cd - change working directory/
				*************************************************************/
				if(strncmp(rbuf1,"pwd",3) == 0)
               	{   
                    //printf("%s\n",rbuf1);
                    sprintf(wbuf,"%s","PWD\n");
                    write(sockfd,wbuf,4);
                    continue; 
                }


				/* pwd -  print working directory */
				if (strncmp(rbuf, "pwd", 3) == 0) 
				{
					sprintf(wbuf, "%s", "PWD\n");
					write(sockfd, wbuf, 4);
					continue;
				}

				/*************************************************************
				// 6. code here: quit - quit from ftp server
				*************************************************************/
                if(strncmp(rbuf1,"quit",4) == 0)
                {
                    sprintf(wbuf,"%s","QUIT\n");
                    write(sockfd,wbuf,5);
                    //close(sockfd);
                    if(close(sockfd) <0)
                 	   printf("close error\n");
                    break;
                }

				/*************************************************************
				// 7. code here: get - get file from ftp server
				*************************************************************/
                if(strncmp(rbuf1,"get",3) == 0)
                {
                    tag = 1;            //下载文件标识符
                    //被动传输模式    
                    sprintf(wbuf,"%s","PASV\n");                   
                    //printf("%s\n",s(rbuf1));
                    //char filename[100];
                    s(rbuf1,filename);
                    printf("%s\n",filename);
                    write(sockfd,wbuf,5);
                    continue;
                }
				/*************************************************************
				// 8. code here: put -  put file upto ftp server
				*************************************************************/
                if(strncmp(rbuf1,"put",3) == 0)
                {
                    tag = 3;            //上传文件标识符
                    sprintf(wbuf,"%s","PASV\n");
                    //把内容赋值给  读缓冲区
                    st(rbuf1,filename);
                    printf("%s\n",filename);
                    write(sockfd,wbuf,5);
                    continue;
                }
            } 

			write(sockfd, rbuf, nread);
		}
	}
	
	/* data to read from socket */
	if (FD_ISSET(sockfd, &rset)) {
		if ( (nread = recv(sockfd, rbuf, MAXBUF, 0)) < 0)
			printf("recv error\n");
		else if (nread == 0)
			break;
		/* set replycode and wait for user's input */
		if (strncmp(rbuf, "220", 3)==0 || strncmp(rbuf, "530", 3)==0)
		{
			strcat(rbuf,  "your name: ");
			nread += 12;
			replycode = USERNAME;
		}

		/*************************************************************
		// 9. code here: handle other response coming from server
		*************************************************************/
		if(strncmp(rbuf,"331",3) == 0)
       	{
        /*if(write(STDOUT_FILENO,rbuf,nread) != nread)
            printf("write error to stdout\n")*/;
            strcat(rbuf,"your password:");
            nread += 16;
            /*if(write(STDOUT_FILENO,rbuf,nread) != nread)
                printf("write error to stdout\n");*/
            replycode = PASSWORD;
        }
        if(strncmp(rbuf,"230",3) == 0)
        {
            /*if(write(STDOUT_FILENO,rbuf,nread) != nread)
                printf("write error to stdout\n");*/
            replycode = LOGIN;
        }
        if(strncmp(rbuf,"257",3) == 0)
        {
            /*if(write(STDOUT_FILENO,rbuf,nread) != nread)
            printf("write error to stdout\n");*/
            replycode = PATHNAME;  
        }
        if(strncmp(rbuf,"226",3) == 0)
        {
            /*if(write(STDOUT_FILENO,rbuf,nread) != nread)
                printf("write error to stdout\n");*/
            replycode = CLOSEDATA;
        }
        if(strncmp(rbuf,"250",3) == 0)
        {
            /*if(write(STDOUT_FILENO,rbuf,nread) != nread)
                printf("write error to stdout\n");*/
            replycode = ACTIONOK;
        }
        if(strncmp(rbuf,"550",3) == 0)
        {
	        replycode = 550;
        }
        /*if(strncmp(rbuf,"150",3) == 0)
        {
            if(write(STDOUT_FILENO,rbuf,nread) != nread)
                printf("write error to stdout\n");
        }*/    
        //fprintf(stderr,"%d\n",1);
			/* open data connection*/
		if (strncmp(rbuf, "227", 3) == 0) {
			strtosrv(rbuf, host, port);
			fd = cliopen(host, port);
			write(sockfd, wbuf1, nwrite);
			nwrite = 0;
		}
		/* start data transfer */
		if (write(STDOUT_FILENO, rbuf, nread) != nread)
		{
			printf("write error to stdout\n");
		}
	}
	if (close(sockfd) < 0){
		printf("close error\n");
	}

}

/* Read and write as data transfer connection */
void
ftp_list(int sockfd)
{
	int		nread;

	for ( ; ; )
	{
		/* data to read from socket */
		if ( (nread = recv(sockfd, rbuf1, MAXBUF, 0)) < 0)
			printf("recv error\n");
		else if (nread == 0)
			break;

		if (write(STDOUT_FILENO, rbuf1, nread) != nread)
			printf("send error to stdout\n");
	}

	if (close(sockfd) < 0)
		printf("close error\n");
}

/* download file from ftp server */
int	
ftp_get(int sck, char *pDownloadFileName_s)
{
	/*************************************************************
	// 10. code here: 
	*************************************************************/
	int handle = open(pDownloadFileName,O_WRONLY | O_CREAT | O_TRUNC, S_IREAD| S_IWRITE);
    int nread;
    printf("%d\n",handle);
    /*if(handle == -1) 
        return -1;*/
 
     //2. 猜测 ====================应该是将 ssl 接口放在这里，用来传输数据
    
    for(;;)
    {
        if((nread = recv(sck,rbuf1,MAXBUF,0)) < 0)
        {
           printf("receive error\n");
        }
        else if(nread == 0)
       {
           printf("over\n");
           break;
        }
     //   printf("%s\n",rbuf1);
        if(write(handle,rbuf1,nread) != nread)
            printf("receive error from server!");
        if(write(STDOUT_FILENO,rbuf1,nread) != nread)
            printf("receive error from server!");
    }
        if(close(sck) < 0)
            printf("close error\n");
}

/* upload file to ftp server */
int 
ftp_put (int sck, char *pUploadFileName_s)
{
	/*************************************************************
	// 11. code here
	*************************************************************/
	//int c_sock;
    int handle = open(pUploadFileName_s,O_RDWR);
    int nread;
    if(handle == -1)
        return -1;
    //ftp_type(c_sock,"I");
 
    //3. 猜测 ====================应该是将 ssl 接口放在这里，用来传输数据
    for(;;)
    {
        if((nread = read(handle,rbuf1,MAXBUF)) < 0)
        {
             printf("read error!");
        }
        else if(nread == 0)
           break;
        if(write(STDOUT_FILENO,rbuf1,nread) != nread)
             printf("send error!");
        if(write(sck,rbuf1,nread) != nread)
             printf("send error!");
    }
    if(close(sck) < 0)
         printf("close error\n");
}
