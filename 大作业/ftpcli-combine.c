/* header files */
#include <stdio.h>
#include <stdlib.h>
#include <netdb.h> /* getservbyname(), gethostbyname() */
#include <errno.h> /* for definition of errno */
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/fcntl.h>
#include <termios.h>  /* Used to not display password directly, instead using *****   */
#include<sys/time.h> // used to limit the transmission rate, get current time
//int gettimeofday(struct  timeval*tv,struct  timezone *tz )
//int nanosleep(const struct timespec *req, struct timespec *rem);
/*
struct  timeval{
       long  tv_sec;
       long  tv_usec;
};

struct  timezone{
  int tz_minuteswest;
  int tz_dsttime;
};
*/

#define ECHOFLAGS (ECHO | ECHOE | ECHOK | ECHONL)


/* define macros*/
#define MAXBUF             1024        
#define STDIN_FILENO     0            
#define STDOUT_FILENO     1            

/* define FTP reply code */
#define USERNAME     220            
#define PASSWORD      331            
#define LOGIN           230            
#define PATHNAME      257            
#define CLOSEDATA     226       
#define ACTIONOK      250    
//10KB/s
#define SPEEDLIMIT 0.01

/* DefinE global variables */
char *rbuf,*rbuf1,*wbuf,*wbuf1;           /* pointer that is malloc'ed */
char filename[100];
char newfilename[100]; 
char tmp[100];
char dirname[100];            
char *host;                            /* hostname or dotted-decimal string */
struct sockaddr_in servaddr;   
//if binary mode flag =1; else,flag=0
char activeCommand[27] = "PORT 10,128,234,192,50,113\n";
char activeAddress[100] = "10.128.234.192";
int binaryFlag = 1;

//int mygetch();
//int getpasswd(char *passwd, int size);
int set_disp_mode(int fd,int option);
int cliopen(char *host,int port);
int strtosrv(char *str);
void  ftp_list(int sockfd);
int ftp_get(int sck,char *pDownloadFileName);
int ftp_put(int sck,char *pUploadFileName_s);
void cmd_tcp(int sockfd); 
void cmd_tcp_active(int sockfd); 
void cmd_tcp_resume(int sockfd); 
void cmd_tcp_limit(int sockfd);                    
void remove_char_from_string(char c, char *str);
void add_char_from_string(char c, char *str);
//函数set_disp_mode用于控制是否开启输入回显功能
//如果option为0，则关闭回显，为1则打开回显
int set_disp_mode(int fd,int option)
{
   int err;
   struct termios term;
   if(tcgetattr(fd,&term)==-1){
     perror("Cannot get the attribution of the terminal");
     return 1;
   }
   if(option)
        term.c_lflag|=ECHOFLAGS;
   else
        term.c_lflag &=~ECHOFLAGS;
   err=tcsetattr(fd,TCSAFLUSH,&term);
   if(err==-1 && err==EINTR){
        perror("Cannot set the attribution of the terminal");
        return 1;
   }
   return 0;
}

//remove \r
void remove_char_from_string(char c, char *str)
{
    int i=0;
    int len = strlen(str)+1;

    for(i=0; i<len; i++)
    {
        if(str[i] == c)
        {
            // Move all the char following the char "c" by one to the left.
            strncpy(&str[i],&str[i+1],len-i);
        }
    }
}

//add \r
void add_char_from_string(char c, char *str)
{
    int i=0;
    int len = strlen(str)+1;

    char *tempStr;
    //copy the string
    strncpy(&tempStr[0],&str[0],len);


    for(i=0; i<len; i++)
    {
        if(str[i] == '\n')
        {
            // Move all the char following the char "c" by one to the right, then set \r to current.
            strncpy(&str[i+1],&tempStr[i],len-i);
            str[i] = '\r';
        }
    }
}


int main(int argc,char *argv[])
{
    int fd;
    int mode;
    // Judge whether missing parameters
    if(0 != argc -3)
    {
        printf("%s\n","missing <hostname> or <mode>");
        exit(0);
    }

    //define host and port
    host = argv[1];
    mode = atoi(argv[2]);
    int port = 21;
   

    /*****************************************************************
    //1. code here: Allocate the read and write buffers before open().
    // memory allocation for the read and writer buffers for maximum size
    *****************************************************************/
    rbuf = (char *)malloc(MAXBUF*sizeof(char));
    rbuf1 = (char *)malloc(MAXBUF*sizeof(char));
    wbuf = (char *)malloc(MAXBUF*sizeof(char));
    wbuf1 = (char *)malloc(MAXBUF*sizeof(char));

    //Initiate the socket to communication
    fd = cliopen(host,port);
    if(mode == 0){
      cmd_tcp(fd);  
    }
    else if (mode == 1){
      cmd_tcp_active(fd);
    }
    else if (mode == 2){
      cmd_tcp_resume(fd);
    }
    else if (mode == 3){
      cmd_tcp_limit(fd);
    }
    else{
      printf("Please type in number 0-3 0 for passive mode, 1 for active mode,2 for resume mode, 3 for speed limit mode\n");
    }
    exit(0);
}



int dataSock(char *host,int port){
  //printf("正在开启data port\n");
  int sock;
  struct sockaddr_in severAddr;
  struct sockaddr_in clientAddr;
  unsigned int cliAddrLen;
  char recvBuffer[MAXBUF] = "";
  unsigned short severPort;
  int recvMsgSize;
  

  severPort = atoi("12913");
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
  //printf("已经开启data port");
  return sock;

  
}

/* Establish a TCP connection from client to server */
int cliopen(char *host,int port)
{   
    //control and data socket
    int control_sock;
    struct hostent *ht = NULL;

    control_sock = socket(AF_INET,SOCK_STREAM,0);
    if(control_sock < 0)
    {
       printf("socket error\n");
       return -1;
    }

    //将IP地址进行转换，变为FTP地址结构体
    ht = gethostbyname(host);
    if(!ht)
    { 
        return -1;
    }
    
   //connect to server, return socket
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

//匹配下载的文件名
int s(char *str,char *s2)
{
    //char s1[100];
     
    return sscanf(str," get %s",s2) == 1;
   
}

//匹配上传的文件名
int st(char *str,char *s1)
{
    
    return sscanf(str," put %s",s1) == 1;
}


/*
   Compute server's port by a pair of integers and store it in char *port
   Get server's IP address and store it in char *host
*/
int strtosrv(char *str)
{
  /*************************************************************
  //3. code here to compute the port number for data connection
  EG:10,3,255,85,192,181  192*256+181 = 49333
  *************************************************************/
   int addr[6];
   //divide the string in to different parts
   sscanf(str,"%*[^(](%d,%d,%d,%d,%d,%d)",&addr[0],&addr[1],&addr[2],&addr[3],&addr[4],&addr[5]);
   //clear the host
   bzero(host,strlen(host));
   sprintf(host,"%d.%d.%d.%d",addr[0],addr[1],addr[2],addr[3]);
   int port = addr[4]*256 + addr[5];
   return port;
}

/* Read and write as data transfer connection */
void ftp_list(int sockfd)
{
    int nread;
    for( ; ; )
    {
        /* data to read from socket */
        if((nread = recv(sockfd,rbuf1,MAXBUF,0)) < 0)
        {
            printf("recv error\n");
        }
        else if(nread == 0)
        {
            //printf("over\n");
            break;
        }
        if(write(STDOUT_FILENO,rbuf1,nread) != nread)
            printf("send error to stdout\n");
        /*else
            printf("read something\n");*/
    }
    if(close(sockfd) < 0)
        printf("close error\n");
}

/*
void checkSpeed(){
  int stopTime = 1;
  if (curSpeed>SPEEDLIMIT){

  }
}
*/

/* download file from ftp server */
int ftp_get(int sck,char *pDownloadFileName)
{
   int handle = open(pDownloadFileName,O_WRONLY | O_CREAT | O_TRUNC, S_IREAD| S_IWRITE);
   int nread;
   printf("%d\n",handle);
   /*if(handle == -1) 
       return -1;*/
   struct timeval start, end;
   
   for(;;)
   {  

       //checkSpeed
       //gettimeofday( &start, NULL );

       if((nread = recv(sck,rbuf1,MAXBUF,0)) < 0)
       {  
          printf("receive error\n");
       }
       else if(nread == 0)
       {
          printf("over\n");
          break;
       }

       //如果是ascii 模式
       if(binaryFlag == 0){
          remove_char_from_string('\r',rbuf1);
       }
       if(write(handle,rbuf1,nread) != nread)
           printf("receive error from server!");

      /*  Not print file content to console
       if(write(STDOUT_FILENO,rbuf1,nread) != nread)
           printf("receive error from server!");
      */
         
   }
        bzero(rbuf1,strlen(rbuf1));
       if(close(sck) < 0)
           printf("close error\n");
      return 0;
}

/* upload file to ftp server */
int ftp_put(int sck,char *pUploadFileName_s)
{
   //int c_sock;
   int handle = open(pUploadFileName_s,O_RDWR);
   int nread;
   //error open
   if(handle == -1)
       return -1;
   //ftp_type(c_sock,"I");

   for(;;)
   {
       if((nread = read(handle,rbuf1,MAXBUF)) < 0)
       {
            printf("read error!");
       }
       else if(nread == 0)
          break;
       /*
       if(write(STDOUT_FILENO,rbuf1,nread) != nread)
            printf("send error!");
      */
        if(binaryFlag == 0){
          add_char_from_string('\r',rbuf1);
       }
       if(write(sck,rbuf1,nread) != nread)
            printf("send error!");
   }
   bzero(rbuf1,strlen(rbuf1));
   if(close(sck) < 0)
        printf("close error\n");
    return 0;
}


/* Read and write as command connection for passive mode*/
void cmd_tcp(int sockfd)
{
    int maxfdp1,nread,nwrite,fd,replycode,tag=0,data_sock;
    int port;
    int newfilenameLen;
    char *pathname;
    fd_set rset;                
    FD_ZERO(&rset);                
    maxfdp1 = sockfd + 1;        /* check descriptors [0..sockfd] */

    for(;;)
    {
        //1.将  标准输入加入 可读文件描述符集合
         FD_SET(STDIN_FILENO,&rset);
        //2.将  命令套接字   加入可读文件描述符集合
         FD_SET(sockfd,&rset);

        //3.监听读事件
         if(select(maxfdp1,&rset,NULL,NULL,NULL)<0)
         {
             printf("select error\n");
         } 
         //4.判断标准输入是否有读事件
         if(FD_ISSET(STDIN_FILENO,&rset))
          // && replycode  != PASSWORD
         {
             //5.清空读缓冲区 和 写缓冲区
              bzero(wbuf,MAXBUF);          //zero
              bzero(rbuf1,MAXBUF);
              
              
              //set_disp_mode(STDIN_FILENO,1);  

              if((nread = read(STDIN_FILENO,rbuf1,MAXBUF)) <0)
                   printf("read error from stdin\n");
              nwrite = nread + 5;


            /* send username */
              if(replycode == USERNAME) 
              {
                  sprintf(wbuf,"USER %s",rbuf1);
              
                 if(write(sockfd,wbuf,nwrite) != nwrite)
                 {
                     printf("write error\n");
                 }
                 //printf("%s\n",wbuf);
                 //memset(rbuf1,0,sizeof(rbuf1));
                 //memset(wbuf,0,sizeof(wbuf));
                 //printf("1:%s\n",wbuf);
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
              if(replycode == 550 || replycode == LOGIN || replycode == CLOSEDATA || replycode == PATHNAME || replycode == ACTIONOK)
              {
                /* pwd -  print working directory */
                if(strncmp(rbuf1,"pwd",3) == 0)
                {   
                     //printf("%s\n",rbuf1);
                     sprintf(wbuf,"%s","PWD\n");
                     write(sockfd,wbuf,4);
                     continue; 
                 }
                 else if(strncmp(rbuf1,"quit",4) == 0)
                 {
                     sprintf(wbuf,"%s","QUIT\n");
                     write(sockfd,wbuf,5);
                     //close(sockfd);
                    if(close(sockfd) <0)
                       printf("close error\n");
                    printf("221 Goodbye.");
                    break;
                 }
                  /*************************************************************
                  // 5. code here: cd - change working directory/
                  *************************************************************/                 
                 else if(strncmp(rbuf1,"cd",2) == 0 && strlen(rbuf1)!=3)
                 {
                     //sprintf(wbuf,"%s","PASV\n");
                     sscanf(rbuf1,"%s %s", tmp, dirname);
                     //printf("%s\n", dirname);
                     int dirnameLen= strlen(dirname);
                     //if not, the final character will be the \000
                     //dirname[dirnameLen] = '\n';
                     sprintf(wbuf,"CWD %s\n",dirname);
                     write(sockfd,wbuf,strlen(wbuf));
                     
                     //sprintf(wbuf1,"%s","CWD\n");
                     
                     continue;
                 }

                 // mkdir function
                 else if(strncmp(rbuf1,"mkdir",5) == 0 && strlen(rbuf1)!=6)
                 {
                     //sprintf(wbuf,"%s","PASV\n");
                     sscanf(rbuf1,"%s %s", tmp, dirname);
                     //printf("%s\n", dirname);
                     int dirnameLen= strlen(dirname);
                     //if not, the final character will be the \000
                     //dirname[dirnameLen] = '\n';
                     sprintf(wbuf,"MKD %s\n",dirname);
                     write(sockfd,wbuf,strlen(wbuf));
                     //sprintf(wbuf1,"%s","CWD\n");
                     
                     continue;
                 }

                 else if(strncmp(rbuf1,"delete",6) == 0 && strlen(rbuf1)!=7)
                 {
                     sscanf(rbuf1,"%s %s", tmp, filename);
                     //printf("%s\n", dirname);
                     int filenameLen= strlen(filename);
                     //if not, the final character will be the \000
                     //filename[filenameLen] = '\n';
                     sprintf(wbuf,"DELE %s\n",filename);
                     write(sockfd,wbuf,strlen(wbuf));
                     nwrite = 0;
                     
                     continue;   
                 }


                 /* ls - list files and directories*/
                 else if(strncmp(rbuf1,"ls",2) == 0)
                 {
                     tag = 2;            //显示文件 标识符
                     //printf("%s\n",rbuf1);
                     sprintf(wbuf,"%s","PASV\n");
                     //printf("%s\n",wbuf);
                     write(sockfd,wbuf,5);
                     //read
                     //sprintf(wbuf1,"%s","LIST -al\n");
                     nwrite = 0;
                     //write(sockfd,wbuf1,nwrite);
                     //ftp_list(sockfd);
                     continue;    
                 }
                  /*************************************************************
                  // 7. code here: get - get file from ftp server
                  *************************************************************/
                 else if(strncmp(rbuf1,"get",3) == 0 && strlen(rbuf1)!=4)
                 {
                     tag = 1;            //下载文件标识符

                     //被动传输模式    
                     sprintf(wbuf,"%s","PASV\n");                   
                     //printf("%s\n",s(rbuf1));
                     //char filename[100];
                     s(rbuf1,filename);
                     
                     //printf("%s\n",filename);
                     write(sockfd,wbuf,5);
                     continue;
                 }
                  /*************************************************************
                  // 8. code here: put -  put file upto ftp server
                  *************************************************************/                 
                 else if(strncmp(rbuf1,"put",3) == 0 && strlen(rbuf1)!=4)
                 {
                     tag = 3;            //上传文件标识符
                     sprintf(wbuf,"%s","PASV\n");

                    //把内容赋值给  读缓冲区
                     st(rbuf1,filename);
                     //printf("%s\n",filename);
                     write(sockfd,wbuf,5);
                     continue;
                 }

                 //binary mode and ascii mode
                else if(strncmp(rbuf1,"binary",6) == 0)
                 {
                     sprintf(wbuf,"TYPE %s","I\n");
                     write(sockfd,wbuf,7);
                     binaryFlag = 1;
                     continue;
                 }
                 else if(strncmp(rbuf1,"ascii",5) == 0)
                 {
                     sprintf(wbuf,"TYPE %s","A\n");
                     write(sockfd,wbuf,7);
                     binaryFlag = 0;
                     continue;
                 }

                 else if(strncmp(rbuf1,"rename",6) == 0 && strlen(rbuf1)!=7)
                 {
                     //sprintf(wbuf,"%s","PASV\n");
                     sscanf(rbuf1,"%s %s %s", tmp, filename,newfilename);
                     
                     //printf("%s\n", dirname);
                     int filenameLen= strlen(filename);
                     newfilenameLen = strlen(newfilename);
                     
                     if (newfilenameLen == 0){
                      printf("Please type in new file name!\n");
                      replycode = 550;

                     }
                     else{
                         //if not, the final character will be the \000
                     filename[filenameLen] = '\n';
                     newfilename[newfilenameLen] = '\n';


                     sprintf(wbuf,"RNFR %s",filename);
                     write(sockfd,wbuf,filenameLen+6);

                     }
                     
                     

                     nwrite = 0;
                     //sprintf(wbuf1,"%s","CWD\n");
                     
                     continue;
                 }
                 else{
                  //write(sockfd,rbuf1,strlen(rbuf1));
                  //wbuf="Invalid input";
                 printf("This is an Invalid input!!!!!!\n");
                 //保持被动传输模式    
                 tag=0;
                 replycode = 550;
                     //sprintf(wbuf,"%s","PWD\n");                   
                     //write(sockfd,wbuf,4);
                 
                  continue;
                 }




              } 
                    /*if(close(sockfd) <0)
                       printf("close error\n");*/
         }
         if(FD_ISSET(sockfd,&rset))
         {
         //9.清空读缓冲区 和 写缓冲区
             bzero(rbuf,strlen(rbuf));
         //10.读套接字中的内容

             if((nread = recv(sockfd,rbuf,MAXBUF,0)) <0)
                  printf("recv error\n");
             else if(nread == 0)
               break;

             //printf("%s",rbuf);

             if(strncmp(rbuf,"220",3) ==0 || strncmp(rbuf,"530",3)==0)
             {
                /*if(write(STDOUT_FILENO,rbuf,nread) != nread)
                    printf("write error to stdout\n");*/

                 //链接字符串


                 //printf("your name:");
                 //getpasswd(rbuf, 20);
                 strcat(rbuf,"your name:");
                
                 nread += 12;
                 replycode = USERNAME;
             }
             if(strncmp(rbuf,"331",3) == 0)
             {
                /*if(write(STDOUT_FILENO,rbuf,nread) != nread)
                    printf("write error to stdout\n")*/;
                
                strcat(rbuf,"your password:\n");
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

             // Ready to rename
             if(strncmp(rbuf,"350",3) == 0)
             {
                bzero(wbuf,strlen(wbuf));
                sprintf(wbuf,"RNTO %s",newfilename);
                write(sockfd,wbuf,newfilenameLen+6);
                replycode = 350;
             }
             
             /*if(strncmp(rbuf,"150",3) == 0)
             {
                if(write(STDOUT_FILENO,rbuf,nread) != nread)
                    printf("write error to stdout\n");
             }*/    
             //fprintf(stderr,"%d\n",1);
             if(strncmp(rbuf,"227",3) == 0)
             {
                //printf("%d\n",1);
                /*if(write(STDOUT_FILENO,rbuf,nread) != nread)
                   printf("write error to stdout\n");*/

                //获取服务器返回的 接收数据的端口，和地址
                int port1 = strtosrv(rbuf);
                
                //现在主机地址和端口号将不被打印出来
                //printf("%d\n",port1);
                //printf("%s\n",host);
                printf("Retrieving Data...\n");

                //创建新的传输数据的套接字?
                //1. 猜测 ====================应该是将 ssl 接口放在这里，用来传输数据
                data_sock = cliopen(host,port1);
        

                if(tag == 2)
                {
                   write(sockfd,"LIST\n",strlen("LIST\n"));
                   ftp_list(data_sock);
                   /*if(write(STDOUT_FILENO,rbuf,nread) != nread)
                       printf("write error to stdout\n");*/
                   
                }
                //else if(strncmp(rbuf1,"get",3) == 0)
                else if(tag == 1)
                {   

                    //下载文件
                    //sprintf(wbuf,"%s","RETR\n");
                    //printf("%s\n",wbuf);
                    //int str = strlen(filename);
                    //printf("%d\n",str);

                    //断点续传检测



                    sprintf(wbuf,"RETR %s\n",filename);

                    //printf("%s\n",wbuf);
                    //int p = 5 + str + 1;

                    //命令套接字中写入  下载文件命令
                    write(sockfd,wbuf,strlen(wbuf));

                    //9.清空读缓冲区 和 写缓冲区
                    bzero(rbuf,strlen(rbuf));
                      //10.读套接字中的内容

                    if((nread = recv(sockfd,rbuf,MAXBUF,0)) <0)
                        printf("recv error\n");

                    //printf("%s",rbuf);
                    if(strncmp(rbuf,"550",3) == 0)
                    {
                      replycode = 550;
                    }
                    // 如果有这个文件，就下载
                    else{
                      ftp_get(data_sock,filename);
                    }
                    //printf("%d\n",write(sockfd,wbuf,strlen(wbuf)));
                    //printf("%d\n",p);

                     
                    
                }
                else if(tag == 3)
                {
                    // 上传文件
                    sprintf(wbuf,"STOR %s\n",filename);
                    //printf("%s",wbuf);
                    int handle = open(filename,O_RDWR);
                    //printf("%s",wbuf);

                    //Handle file not exist error
                    if (handle != -1)
                    {
                      close(handle);
                      write(sockfd,wbuf,strlen(wbuf));
                      ftp_put(data_sock,filename);
                    }
                    else{
                      bzero(wbuf,strlen(wbuf));
                      printf("File not exist\n");
                      printf("%s",wbuf);
                    }
                       //int c_sock;
                     
                    
                    
                }
                nwrite = 0;
             }
             /*if(strncmp(rbuf,"150",3) == 0)
             {
                 if(write(STDOUT_FILENO,rbuf,nread) != nread)
                     printf("write error to stdout\n");
             }*/
             //printf("%s\n",rbuf);
             if(write(STDOUT_FILENO,rbuf,nread) != nread)
                 printf("write error to stdout\n");
              

              //如果该输密码了，让输入不可见
              if (replycode == PASSWORD)
              {
                set_disp_mode(STDIN_FILENO,0);  
              }
              else{
                set_disp_mode(STDIN_FILENO,1);  
              }

             /*else 
                 printf("%d\n",-1);*/            
         }
    }
}



/* Read and write as data transfer connection */
void ftp_list_active(int sockfd)
{
    int nread;
    struct sockaddr_in clientAddr;
    unsigned int cliAddrLen;
    //printf("%d",tmp);
    //printf("%d",sockfd);
    int newsockfd = 0;
        cliAddrLen = sizeof(clientAddr);
        if((newsockfd = accept(sockfd, (struct sockaddr *) &clientAddr, &cliAddrLen)) < 0)
        {//create a new socket
          printf("accept() failed.\n");
        exit(1);
      }

    for( ; ; )
    {   

        /* data to read from socket */
        if((nread = recv(newsockfd,rbuf1,MAXBUF,0)) < 0)
        {
            printf("recv error\n");
        }
        else if(nread == 0)
        {
            //printf("over\n");
            break;
        }
        if(write(STDOUT_FILENO,rbuf1,nread) != nread)
            printf("send error to stdout\n");
        /*else
            printf("read something\n");*/
          printf("%d",nread);
    }
    if(close(sockfd) < 0)
        printf("close error\n");
    if(close(newsockfd) < 0)
        printf("close error\n");
}

/*
void checkSpeed(){
  int stopTime = 1;
  if (curSpeed>SPEEDLIMIT){

  }
}
*/

/* download file from ftp server */
int ftp_get_active(int sck,char *pDownloadFileName)
{
   int handle = open(pDownloadFileName,O_WRONLY | O_CREAT | O_TRUNC, S_IREAD| S_IWRITE);
   int nread;
   //printf("%d\n",handle);
   /*if(handle == -1) 
       return -1;*/
   
   struct sockaddr_in clientAddr;
    unsigned int cliAddrLen;
    //printf("%d",tmp);
    //printf("%d",sockfd);
    int newsockfd = 0;
        cliAddrLen = sizeof(clientAddr);
        if((newsockfd = accept(sck, (struct sockaddr *) &clientAddr, &cliAddrLen)) < 0)
        {//create a new socket
          printf("accept() failed.\n");
        exit(1);
      }

   for(;;)
   {  


       if((nread = recv(newsockfd,rbuf1,MAXBUF,0)) < 0)
       {  
          printf("receive error\n");
       }
       else if(nread == 0)
       {
          printf("over\n");
          break;
       }
       //printf("%s\n",rbuf1);
       if(write(handle,rbuf1,nread) != nread)
           printf("receive error from server!");

      /*  Not print file content to console
       if(write(STDOUT_FILENO,rbuf1,nread) != nread)
           printf("receive error from server!");
      */
   }
       if(close(sck) < 0)
           printf("close error\n");
        if(close(newsockfd) < 0)
        printf("close error\n");
      return 0;
}

/* upload file to ftp server */
int ftp_put_active(int sck,char *pUploadFileName_s)
{
   //int c_sock;
   int handle = open(pUploadFileName_s,O_RDWR);
   int nread;
   //error open
   if(handle == -1)
       return -1;
   //ftp_type(c_sock,"I");
    struct sockaddr_in clientAddr;
    unsigned int cliAddrLen;
    //printf("%d",tmp);
    //printf("%d",sockfd);
    int newsockfd = 0;
        cliAddrLen = sizeof(clientAddr);
        if((newsockfd = accept(sck, (struct sockaddr *) &clientAddr, &cliAddrLen)) < 0)
        {//create a new socket
          printf("accept() failed.\n");
        exit(1);
      }
   for(;;)
   {
       if((nread = read(handle,rbuf1,MAXBUF)) < 0)
       {
            printf("read error!");
       }
       else if(nread == 0)
          break;
       /*
       if(write(STDOUT_FILENO,rbuf1,nread) != nread)
            printf("send error!");

      */
       if(write(newsockfd,rbuf1,nread) != nread)
            printf("send error!");
   }
   bzero(rbuf1,strlen(rbuf1));
   
    if(close(sck) < 0)
        printf("close error\n");
    if(close(newsockfd) < 0)
        printf("close error\n");

    return 0;
}


/* Read and write as command connection */
void cmd_tcp_active(int sockfd)
{
    int maxfdp1,nread,nwrite,fd,replycode,tag=0,data_sock;
    int port;
    int newfilenameLen;
    char *pathname;
    fd_set rset;                
    FD_ZERO(&rset);                
    maxfdp1 = sockfd + 1;        /* check descriptors [0..sockfd] */

    for(;;)
    {
        //1.将  标准输入加入 可读文件描述符集合
         FD_SET(STDIN_FILENO,&rset);
        //2.将  命令套接字   加入可读文件描述符集合
         FD_SET(sockfd,&rset);

        //3.监听读事件
         if(select(maxfdp1,&rset,NULL,NULL,NULL)<0)
         {
             printf("select error\n");
         } 
         //4.判断标准输入是否有读事件
         if(FD_ISSET(STDIN_FILENO,&rset))
          // && replycode  != PASSWORD
         {
             //5.清空读缓冲区 和 写缓冲区
              bzero(wbuf,MAXBUF);          //zero
              bzero(rbuf1,MAXBUF);
              
              
              //set_disp_mode(STDIN_FILENO,1);  

              if((nread = read(STDIN_FILENO,rbuf1,MAXBUF)) <0)
                   printf("read error from stdin\n");
              nwrite = nread + 5;


            /* send username */
              if(replycode == USERNAME) 
              {
                  sprintf(wbuf,"USER %s",rbuf1);
              
                 if(write(sockfd,wbuf,nwrite) != nwrite)
                 {
                     printf("write error\n");
                 }
                 //printf("%s\n",wbuf);
                 //memset(rbuf1,0,sizeof(rbuf1));
                 //memset(wbuf,0,sizeof(wbuf));
                 //printf("1:%s\n",wbuf);
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
              if(replycode == 550 || replycode == 553 || replycode == LOGIN || replycode == CLOSEDATA || replycode == PATHNAME || replycode == ACTIONOK)
              {
                /* pwd -  print working directory */
                if(strncmp(rbuf1,"pwd",3) == 0)
                {   
                     //printf("%s\n",rbuf1);
                     sprintf(wbuf,"%s","PWD\n");
                     write(sockfd,wbuf,4);
                     continue; 
                 }
                 else if(strncmp(rbuf1,"quit",4) == 0)
                 {
                     sprintf(wbuf,"%s","QUIT\n");
                     write(sockfd,wbuf,5);
                     //close(sockfd);
                    if(close(sockfd) <0)
                       printf("close error\n");
                    printf("221 Goodbye.");
                    break;
                 }
                  /*************************************************************
                  // 5. code here: cd - change working directory/
                  *************************************************************/                 
                 else if(strncmp(rbuf1,"cd",2) == 0)
                 {
                     sscanf(rbuf1,"%s %s", tmp, dirname);
                     //printf("%s\n", dirname);
                     int dirnameLen= strlen(dirname);
                     //if not, the final character will be the \000
                     //dirname[dirnameLen] = '\n';
                     sprintf(wbuf,"CWD %s\n",dirname);
                     write(sockfd,wbuf,strlen(wbuf));
                     
                     //sprintf(wbuf1,"%s","CWD\n");
                     
                     continue;
                 }

                 // mkdir function
                 else if(strncmp(rbuf1,"mkdir",5) == 0)
                 {
                     sscanf(rbuf1,"%s %s", tmp, dirname);
                     //printf("%s\n", dirname);
                     int dirnameLen= strlen(dirname);
                     //if not, the final character will be the \000
                     //dirname[dirnameLen] = '\n';
                     sprintf(wbuf,"MKD %s\n",dirname);
                     write(sockfd,wbuf,strlen(wbuf));
                     //sprintf(wbuf1,"%s","CWD\n");
                     
                     continue;
                 }

                 else if(strncmp(rbuf1,"delete",6) == 0)
                 {
                     tag = 4; //删除文件标识符
                     //printf("%s\n",rbuf1);
                     sprintf(wbuf,"%s",activeCommand);
                     //printf("%s\n",wbuf);
                     write(sockfd,wbuf,strlen(activeCommand));
                     //read
                     //sprintf(wbuf1,"%s","LIST -al\n");
                     nwrite = 0;
                     //write(sockfd,wbuf1,nwrite);
                     //ftp_list(sockfd);
                     continue;   
                 }


                 /* ls - list files and directories*/
                 else if(strncmp(rbuf1,"ls",2) == 0)
                 {
                     tag = 2;            //显示文件 标识符
                     //printf("%s\n",rbuf1);
                     sprintf(wbuf,"%s",activeCommand);
                     //printf("%s\n",wbuf);
                     write(sockfd,wbuf,strlen(activeCommand));
                     //read
                     //sprintf(wbuf1,"%s","LIST -al\n");
                     nwrite = 0;
                     //write(sockfd,wbuf1,nwrite);
                     //ftp_list(sockfd);
                     continue;    
                 }
                  /*************************************************************
                  // 7. code here: get - get file from ftp server
                  *************************************************************/
                 else if(strncmp(rbuf1,"get",3) == 0)
                 {
                     tag = 1;            //下载文件标识符

                     //被动传输模式    
                     sprintf(wbuf,"%s",activeCommand);                   
                     //printf("%s\n",s(rbuf1));
                     //char filename[100];
                     s(rbuf1,filename);
                     
                     //printf("%s\n",filename);
                     write(sockfd,wbuf,strlen(activeCommand));
                     continue;
                 }
                  /*************************************************************
                  // 8. code here: put -  put file upto ftp server
                  *************************************************************/                 
                 else if(strncmp(rbuf1,"put",3) == 0)
                 {
                     tag = 3;            //上传文件标识符
                     sprintf(wbuf,"%s",activeCommand);

                    //把内容赋值给  读缓冲区
                     st(rbuf1,filename);
                     //printf("%s\n",filename);
                     write(sockfd,wbuf,strlen(activeCommand));
                     continue;
                 }

                 //binary mode and ascii mode
                else if(strncmp(rbuf1,"binary",6) == 0)
                 {
                     sprintf(wbuf,"TYPE %s","I\n");

                    //把内容赋值给  读缓冲区
                     st(rbuf1,filename);
                     printf("%s\n",filename);
                     write(sockfd,wbuf,7);
                     continue;
                 }
                 else if(strncmp(rbuf1,"ascii",5) == 0)
                 {
                     sprintf(wbuf,"TYPE %s","A\n");

                    //把内容赋值给  读缓冲区
                     st(rbuf1,filename);
                     printf("%s\n",filename);
                     write(sockfd,wbuf,7);
                     continue;
                 }

                 else if(strncmp(rbuf1,"rename",6) == 0)
                 {
                     //sprintf(wbuf,"%s","PASV\n");
                     sscanf(rbuf1,"%s %s %s", tmp, filename,newfilename);
                     //printf("%s\n", dirname);
                     int filenameLen= strlen(filename);
                     newfilenameLen = strlen(newfilename);
                     //if not, the final character will be the \000
                     filename[filenameLen] = '\n';
                     newfilename[newfilenameLen] = '\n';
                     sprintf(wbuf,"RNFR %s",filename);
                     write(sockfd,wbuf,filenameLen+6);

                     

                     nwrite = 0;
                     //sprintf(wbuf1,"%s","CWD\n");
                     
                     continue;
                 }
                 else{
                  write(sockfd,rbuf1,strlen(rbuf1));
                  continue;
                 }




              } 
                    /*if(close(sockfd) <0)
                       printf("close error\n");*/
         }
         if(FD_ISSET(sockfd,&rset))
         {
         //9.清空读缓冲区 和 写缓冲区
             bzero(rbuf,strlen(rbuf));
         //10.读套接字中的内容

             if((nread = recv(sockfd,rbuf,MAXBUF,0)) <0)
                  printf("recv error\n");
             else if(nread == 0)
               break;

             //printf("%s",rbuf);

             if(strncmp(rbuf,"220",3) ==0 || strncmp(rbuf,"530",3)==0)
             {
                /*if(write(STDOUT_FILENO,rbuf,nread) != nread)
                    printf("write error to stdout\n");*/

                 //链接字符串


                 //printf("your name:");
                 //getpasswd(rbuf, 20);
                 strcat(rbuf,"your name:");
                
                 nread += 12;
                 replycode = USERNAME;
             }
             if(strncmp(rbuf,"331",3) == 0)
             {
                /*if(write(STDOUT_FILENO,rbuf,nread) != nread)
                    printf("write error to stdout\n")*/;
                
                strcat(rbuf,"your password:\n");
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
             //not allow to create file
             if(strncmp(rbuf,"553",3) == 0)
             {
                replycode = 553;
             }

             // Ready to rename
             if(strncmp(rbuf,"350",3) == 0)
             {
                bzero(wbuf,strlen(wbuf));
                sprintf(wbuf,"RNTO %s",newfilename);
                write(sockfd,wbuf,newfilenameLen+6);
                replycode = 350;
             }
             
             /*if(strncmp(rbuf,"150",3) == 0)
             {
                if(write(STDOUT_FILENO,rbuf,nread) != nread)
                    printf("write error to stdout\n");
             }*/    
             //fprintf(stderr,"%d\n",1);
             if(strncmp(rbuf,"200 PORT",8) == 0)
             {
                //printf("%d\n",1);
                /*if(write(STDOUT_FILENO,rbuf,nread) != nread)
                   printf("write error to stdout\n");*/

                //获取服务器返回的 接收数据的端口，和地址
                int port1 = 12913;
                printf("%d\n",port1);
                printf("%s\n",host);

                //创建新的传输数据的套接字?
                //1. 猜测 ====================应该是将 ssl 接口放在这里，用来传输数据
                data_sock = dataSock(host,port1);
        


                if(tag == 2)
                {
                   write(sockfd,"LIST\n",strlen("LIST\n"));
                   ftp_list_active(data_sock);
                   /*if(write(STDOUT_FILENO,rbuf,nread) != nread)
                       printf("write error to stdout\n");*/
                   close(data_sock);
                }
                //else if(strncmp(rbuf1,"get",3) == 0)
                else if(tag == 1)
                {     
                  //下载文件 
                    //sprintf(wbuf,"%s","RETR\n");
                    //printf("%s\n",wbuf);
                    //int str = strlen(filename);
                    //printf("%d\n",str);
                    sprintf(wbuf,"RETR %s\n",filename);

                    //printf("%s\n",wbuf);
                    //int p = 5 + str + 1;

                    //命令套接字中写入  下载文件命令
                    write(sockfd,wbuf,strlen(wbuf));

                    //9.清空读缓冲区 和 写缓冲区
                    bzero(rbuf,strlen(rbuf));
                      //10.读套接字中的内容

                    if((nread = recv(sockfd,rbuf,MAXBUF,0)) <0)
                        {
                          printf("recv error\n");
                          exit(1);
                        }

                    //printf("%s",rbuf);
                    if(strncmp(rbuf,"550",3) == 0)
                    {
                      replycode = 550;
                    }
                    // 如果有这个文件，就下载
                    else{
                      ftp_get_active(data_sock,filename);
                    }
                    //printf("%d\n",write(sockfd,wbuf,strlen(wbuf)));
                    //printf("%d\n",p);

                    close(data_sock);
                    
                }
                else if(tag == 3)
                {
                    // 上传文件
                    sprintf(wbuf,"STOR %s\n",filename);
                    //printf("%s",wbuf);
                    int handle = open(filename,O_RDWR);
                    //printf("%s",wbuf);

                    //Handle file not exist error
                    if (handle != -1)
                    {
                      close(handle);
                      write(sockfd,wbuf,strlen(wbuf));



                        //9.清空读缓冲区 和 写缓冲区
                      bzero(rbuf,strlen(rbuf));
                        //10.读套接字中的内容

                      if((nread = recv(sockfd,rbuf,MAXBUF,0)) <0)
                          {
                            printf("recv error\n");
                            exit(1);
                          }

                      //printf("%s",rbuf);
                      if(strncmp(rbuf,"553",3) == 0)
                      {
                        replycode = 553;
                      }
                      // 如果允许上传这个文件就上传
                      else{
                        close(handle);
                        ftp_put_active(data_sock,filename);
                      }
                    }
                    else{
                      bzero(wbuf,strlen(wbuf));
                      printf("File not exist\n");
                      printf("%s",wbuf);
                    }
                       //int c_sock;
                     
                    
                    close(data_sock);
                }
                else if (tag == 4){
                     sscanf(rbuf1,"%s %s", tmp, filename);
                     //printf("%s\n", dirname);
                     int filenameLen= strlen(filename);
                     //if not, the final character will be the \000
                     //filename[filenameLen] = '\n';
                     sprintf(wbuf,"DELE %s\n",filename);
                     write(sockfd,wbuf,strlen(wbuf));
                     //sprintf(wbuf1,"%s","CWD\n");
                     close(data_sock);
                     continue;
                }
                nwrite = 0;     
             }
             /*if(strncmp(rbuf,"150",3) == 0)
             {
                 if(write(STDOUT_FILENO,rbuf,nread) != nread)
                     printf("write error to stdout\n");
             }*/
             //printf("%s\n",rbuf);
             if(write(STDOUT_FILENO,rbuf,nread) != nread)
                 printf("write error to stdout\n");
              

              //如果该输密码了，让输入不可见
              if (replycode == PASSWORD)
              {
                set_disp_mode(STDIN_FILENO,0);  
              }
              else{
                set_disp_mode(STDIN_FILENO,1);  
              }

             /*else 
                 printf("%d\n",-1);*/            
         }
    }
}





/* download file from ftp server */
int ftp_get_resume(int sck,char *pDownloadFileName)
{
   int handle = open(pDownloadFileName,O_WRONLY | O_CREAT | O_TRUNC, S_IREAD| S_IWRITE);
   int nread;
   printf("%d\n",handle);
   /*if(handle == -1) 
       return -1;*/
   struct timeval start, end;
   
   for(;;)
   {  

       //checkSpeed
       //gettimeofday( &start, NULL );

       if((nread = recv(sck,rbuf1,MAXBUF,0)) < 0)
       {  
          printf("receive error\n");
       }
       else if(nread == 0)
       {
          printf("over\n");
          break;
       }

       if(write(handle,rbuf1,nread) != nread)
           printf("receive error from server!");

      /*  Not print file content to console
       if(write(STDOUT_FILENO,rbuf1,nread) != nread)
           printf("receive error from server!");
      */
         
   }
        bzero(rbuf1,strlen(rbuf1));
       if(close(sck) < 0)
           printf("close error\n");
      return 0;
}

/* Resume downloading file from ftp server */
int ftp_resume(int sck,char *pDownloadFileName)
{
   int handle = open(pDownloadFileName,O_RDWR | O_APPEND);
   int nread;
   printf("%d\n",handle);
   /*if(handle == -1) 
       return -1;*/
    struct timeval start, end;
    
    printf("Now file is resuming downloading...\n"); 
   for(;;)
   {  

       //checkSpeed
       //gettimeofday( &start, NULL );

       if((nread = recv(sck,rbuf1,MAXBUF,0)) < 0)
       {  
          printf("receive error\n");
       }
       else if(nread == 0)
       {
          printf("over\n");
          break;
       }
       if(write(handle,rbuf1,nread) != nread)
           printf("receive error from server!\n");

      /*  Not print file content to console
       if(write(STDOUT_FILENO,rbuf1,nread) != nread)
           printf("receive error from server!");
      */
         
   }
        bzero(rbuf1,strlen(rbuf1));
       if(close(sck) < 0)
           printf("close error\n");
      return 0;
}

/* upload file to ftp server */
int ftp_put_resume(int sck,char *pUploadFileName_s)
{
   //int c_sock;
   int handle = open(pUploadFileName_s,O_RDWR);
   int nread;
   //error open
   if(handle == -1)
       return -1;
   //ftp_type(c_sock,"I");

   for(;;)
   {
       if((nread = read(handle,rbuf1,MAXBUF)) < 0)
       {
            printf("read error!");
       }
       else if(nread == 0)
          break;
       /*
       if(write(STDOUT_FILENO,rbuf1,nread) != nread)
            printf("send error!");
      */
       if(write(sck,rbuf1,nread) != nread)
            printf("send error!");
   }
   bzero(rbuf1,strlen(rbuf1));
   if(close(sck) < 0)
        printf("close error\n");
    return 0;
}


/* Read and write as command connection */
void cmd_tcp_resume(int sockfd)
{
    int maxfdp1,nread,nwrite,fd,replycode,tag=0,data_sock;
    int port;
    int newfilenameLen;
    char *pathname;
    fd_set rset;                
    FD_ZERO(&rset);                
    maxfdp1 = sockfd + 1;        /* check descriptors [0..sockfd] */

    for(;;)
    {
        //1.将  标准输入加入 可读文件描述符集合
         FD_SET(STDIN_FILENO,&rset);
        //2.将  命令套接字   加入可读文件描述符集合
         FD_SET(sockfd,&rset);

        //3.监听读事件
         if(select(maxfdp1,&rset,NULL,NULL,NULL)<0)
         {
             printf("select error\n");
         } 
         //4.判断标准输入是否有读事件
         if(FD_ISSET(STDIN_FILENO,&rset))
          // && replycode  != PASSWORD
         {
             //5.清空读缓冲区 和 写缓冲区
              bzero(wbuf,MAXBUF);          //zero
              bzero(rbuf1,MAXBUF);
              
              
              //set_disp_mode(STDIN_FILENO,1);  

              if((nread = read(STDIN_FILENO,rbuf1,MAXBUF)) <0)
                   printf("read error from stdin\n");
              nwrite = nread + 5;


            /* send username */
              if(replycode == USERNAME) 
              {
                  sprintf(wbuf,"USER %s",rbuf1);
              
                 if(write(sockfd,wbuf,nwrite) != nwrite)
                 {
                     printf("write error\n");
                 }
                 //printf("%s\n",wbuf);
                 //memset(rbuf1,0,sizeof(rbuf1));
                 //memset(wbuf,0,sizeof(wbuf));
                 //printf("1:%s\n",wbuf);
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
              if(replycode == 550 || replycode == LOGIN || replycode == CLOSEDATA || replycode == PATHNAME || replycode == ACTIONOK)
              {
                /* pwd -  print working directory */
                if(strncmp(rbuf1,"pwd",3) == 0)
                {   
                     //printf("%s\n",rbuf1);
                     sprintf(wbuf,"%s","PWD\n");
                     write(sockfd,wbuf,4);
                     continue; 
                 }
                 else if(strncmp(rbuf1,"quit",4) == 0)
                 {
                     sprintf(wbuf,"%s","QUIT\n");
                     write(sockfd,wbuf,5);
                     //close(sockfd);
                    if(close(sockfd) <0)
                       printf("close error\n");
                    printf("221 Goodbye.");
                    break;
                 }
                  /*************************************************************
                  // 5. code here: cd - change working directory/
                  *************************************************************/                 
                 else if(strncmp(rbuf1,"cd",2) == 0)
                 {
                     //sprintf(wbuf,"%s","PASV\n");
                     sscanf(rbuf1,"%s %s", tmp, dirname);
                     //printf("%s\n", dirname);
                     int dirnameLen= strlen(dirname);
                     //if not, the final character will be the \000
                     //dirname[dirnameLen] = '\n';
                     sprintf(wbuf,"CWD %s\n",dirname);
                     write(sockfd,wbuf,strlen(wbuf));
                     
                     //sprintf(wbuf1,"%s","CWD\n");
                     
                     continue;
                 }

                 // mkdir function
                 else if(strncmp(rbuf1,"mkdir",5) == 0 && strlen(rbuf1)!=5)
                 {
                     //sprintf(wbuf,"%s","PASV\n");
                     sscanf(rbuf1,"%s %s", tmp, dirname);
                     //printf("%s\n", dirname);
                     int dirnameLen= strlen(dirname);
                     //if not, the final character will be the \000
                     //dirname[dirnameLen] = '\n';
                     sprintf(wbuf,"MKD %s\n",dirname);
                     write(sockfd,wbuf,strlen(wbuf));
                     //sprintf(wbuf1,"%s","CWD\n");
                     
                     continue;
                 }

                 else if(strncmp(rbuf1,"delete",6) == 0 && strlen(rbuf1)!=5)
                 {
                     sscanf(rbuf1,"%s %s", tmp, filename);
                     //printf("%s\n", dirname);
                     int filenameLen= strlen(filename);
                     //if not, the final character will be the \000
                     //filename[filenameLen] = '\n';
                     sprintf(wbuf,"DELE %s\n",filename);
                     write(sockfd,wbuf,strlen(wbuf));
                     nwrite = 0;
                     
                     continue;   
                 }


                 /* ls - list files and directories*/
                 else if(strncmp(rbuf1,"ls",2) == 0)
                 {
                     tag = 2;            //显示文件 标识符
                     //printf("%s\n",rbuf1);
                     sprintf(wbuf,"%s","PASV\n");
                     //printf("%s\n",wbuf);
                     write(sockfd,wbuf,5);
                     //read
                     //sprintf(wbuf1,"%s","LIST -al\n");
                     nwrite = 0;
                     //write(sockfd,wbuf1,nwrite);
                     //ftp_list(sockfd);
                     continue;    
                 }
                  /*************************************************************
                  // 7. code here: get - get file from ftp server
                  *************************************************************/
                 else if(strncmp(rbuf1,"get",3) == 0 && strlen(rbuf1)!=3)
                 {
                     tag = 1;            //下载文件标识符

                     //被动传输模式    
                     sprintf(wbuf,"%s","PASV\n");                   
                     //printf("%s\n",s(rbuf1));
                     //char filename[100];
                     s(rbuf1,filename);
                     
                     //printf("%s\n",filename);
                     write(sockfd,wbuf,5);
                     continue;
                 }
                  /*************************************************************
                  // 8. code here: put -  put file upto ftp server
                  *************************************************************/                 
                 else if(strncmp(rbuf1,"put",3) == 0 && strlen(rbuf1)!=3)
                 {
                     tag = 3;            //上传文件标识符
                     sprintf(wbuf,"%s","PASV\n");

                    //把内容赋值给  读缓冲区
                     st(rbuf1,filename);
                     //printf("%s\n",filename);
                     write(sockfd,wbuf,5);
                     continue;
                 }

                 //binary mode and ascii mode
                else if(strncmp(rbuf1,"binary",6) == 0)
                 {
                     sprintf(wbuf,"TYPE %s","I\n");
                     write(sockfd,wbuf,7);
                     continue;
                 }
                 else if(strncmp(rbuf1,"ascii",5) == 0)
                 {
                     sprintf(wbuf,"TYPE %s","A\n");
                     write(sockfd,wbuf,7);
                     continue;
                 }

                 else if(strncmp(rbuf1,"rename",6) == 0 && strlen(rbuf1)!=6)
                 {
                     //sprintf(wbuf,"%s","PASV\n");
                     sscanf(rbuf1,"%s %s %s", tmp, filename,newfilename);
                     //printf("%s\n", dirname);
                     int filenameLen= strlen(filename);
                     newfilenameLen = strlen(newfilename);
                     //if not, the final character will be the \000
                     filename[filenameLen] = '\n';
                     newfilename[newfilenameLen] = '\n';
                     sprintf(wbuf,"RNFR %s",filename);
                     write(sockfd,wbuf,filenameLen+6);

                     

                     nwrite = 0;
                     //sprintf(wbuf1,"%s","CWD\n");
                     
                     continue;
                 }
                 else{
                  //write(sockfd,rbuf1,strlen(rbuf1));
                  //wbuf="Invalid input";
                 printf("Invalid input\n");
                 //保持被动传输模式    
                 tag=0;
                     sprintf(wbuf,"%s","PASV\n");                   
                     write(sockfd,wbuf,5);
                 
                  continue;
                 }




              } 
                    /*if(close(sockfd) <0)
                       printf("close error\n");*/
         }
         if(FD_ISSET(sockfd,&rset))
         {
         //9.清空读缓冲区 和 写缓冲区
             bzero(rbuf,strlen(rbuf));
         //10.读套接字中的内容

             if((nread = recv(sockfd,rbuf,MAXBUF,0)) <0)
                  printf("recv error\n");
             else if(nread == 0)
               break;

             //printf("%s",rbuf);

             if(strncmp(rbuf,"220",3) ==0 || strncmp(rbuf,"530",3)==0)
             {
                /*if(write(STDOUT_FILENO,rbuf,nread) != nread)
                    printf("write error to stdout\n");*/

                 //链接字符串


                 //printf("your name:");
                 //getpasswd(rbuf, 20);
                 strcat(rbuf,"your name:");
                
                 nread += 12;
                 replycode = USERNAME;
             }
             if(strncmp(rbuf,"331",3) == 0)
             {
                /*if(write(STDOUT_FILENO,rbuf,nread) != nread)
                    printf("write error to stdout\n")*/;
                
                strcat(rbuf,"your password:\n");
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

             // Ready to rename
             if(strncmp(rbuf,"350",3) == 0)
             {
                bzero(wbuf,strlen(wbuf));
                sprintf(wbuf,"RNTO %s",newfilename);
                write(sockfd,wbuf,newfilenameLen+6);
                replycode = 350;
             }
             
             /*if(strncmp(rbuf,"150",3) == 0)
             {
                if(write(STDOUT_FILENO,rbuf,nread) != nread)
                    printf("write error to stdout\n");
             }*/    
             //fprintf(stderr,"%d\n",1);
             if(strncmp(rbuf,"227",3) == 0)
             {
                //printf("%d\n",1);
                /*if(write(STDOUT_FILENO,rbuf,nread) != nread)
                   printf("write error to stdout\n");*/

                //获取服务器返回的 接收数据的端口，和地址
                int port1 = strtosrv(rbuf);
                printf("%d\n",port1);
                printf("%s\n",host);
                printf("Retrieving Data...\n");

                //创建新的传输数据的套接字?
                //1. 猜测 ====================应该是将 ssl 接口放在这里，用来传输数据
                data_sock = cliopen(host,port1);
        

                if(tag == 2)
                {
                   write(sockfd,"LIST\n",strlen("LIST\n"));
                   ftp_list(data_sock);
                   /*if(write(STDOUT_FILENO,rbuf,nread) != nread)
                       printf("write error to stdout\n");*/
                   
                }
                //else if(strncmp(rbuf1,"get",3) == 0)
                else if(tag == 1)
                {   

                    //下载文件
                    //sprintf(wbuf,"%s","RETR\n");
                    //printf("%s\n",wbuf);
                    //int str = strlen(filename);
                    //printf("%d\n",str);

                    //断点续传检测
                    FILE * pFile;
                    long size;
                    pFile = fopen (filename,"rb");
                    if (pFile==NULL)
                    {  
                       sprintf(wbuf,"RETR %s\n",filename);
                      //printf("%s\n",wbuf);
                      //int p = 5 + str + 1;

                      //命令套接字中写入  下载文件命令
                      write(sockfd,wbuf,strlen(wbuf));
                      //9.清空读缓冲区 和 写缓冲区
                      bzero(rbuf,strlen(rbuf));
                        //10.读套接字中的内容

                      if((nread = recv(sockfd,rbuf,MAXBUF,0)) <0)
                          printf("recv error\n");
                      //printf("%s",rbuf);
                      if(strncmp(rbuf,"550",3) == 0)
                        {
                          replycode = 550;
                        }
                      // 如果有这个文件，就下载
                        else
                        {
                          ftp_get_resume(data_sock,filename);
                        }
                      }
                    else
                    {
                         fseek (pFile, 0, SEEK_END);   ///将文件指针移动文件结尾
                         size=ftell (pFile); ///求出当前文件指针距离文件开始的字节数
                         fclose (pFile);
                         printf ("Size of this file: %ld bytes.\n",size);


                          sprintf(wbuf,"REST %ld\n",size);
                          write(sockfd,wbuf,strlen(wbuf));

                          //9.清空读缓冲区 和 写缓冲区
                          bzero(rbuf,strlen(rbuf));
                            //10.读套接字中的内容

                          if((nread = recv(sockfd,rbuf,MAXBUF,0)) <0)
                              printf("recv error\n");


                          //printf("%s",rbuf);
                          if(strncmp(rbuf,"350",3) == 0)
                          {
                            //如果不进入binary模式，无法开始传输，会报错550 No support for resume of ASCII transfer
                          sprintf(wbuf,"TYPE I\n");
                          write(sockfd,wbuf,strlen(wbuf));

                          //9.清空读缓冲区 和 写缓冲区
                          bzero(rbuf,strlen(rbuf));
                            //10.读套接字中的内容

                          if((nread = recv(sockfd,rbuf,MAXBUF,0)) <0)
                              printf("recv error\n");
                        
                            sprintf(wbuf,"RETR %s\n",filename);

                            //命令套接字中写入  下载文件命令
                            write(sockfd,wbuf,strlen(wbuf));

                            //9.清空读缓冲区 和 写缓冲区
                            bzero(rbuf,strlen(rbuf));
                              //10.读套接字中的内容

                            if((nread = recv(sockfd,rbuf,MAXBUF,0)) <0)
                                printf("recv error\n");

                            //printf("%s",rbuf);
                            if(strncmp(rbuf,"550",3) == 0)
                              {
                                replycode = 550;
                              }
                            // 如果有这个文件，就下载
                              else
                              {
                                ftp_resume(data_sock,filename);
                              }
                          }
                    }




                    
                    //printf("%d\n",write(sockfd,wbuf,strlen(wbuf)));
                    //printf("%d\n",p);

                     
                    
                }
                else if(tag == 3)
                {
                    // 上传文件
                    sprintf(wbuf,"STOR %s\n",filename);
                    //printf("%s",wbuf);
                    int handle = open(filename,O_RDWR);
                    //printf("%s",wbuf);

                    //Handle file not exist error
                    if (handle != -1)
                    {
                      close(handle);
                      write(sockfd,wbuf,strlen(wbuf));
                      ftp_put_resume(data_sock,filename);
                    }
                    else{
                      bzero(wbuf,strlen(wbuf));
                      printf("File not exist\n");
                      printf("%s",wbuf);
                    }
                       //int c_sock;
                     
                    
                    
                }
                nwrite = 0;
             }
             /*if(strncmp(rbuf,"150",3) == 0)
             {
                 if(write(STDOUT_FILENO,rbuf,nread) != nread)
                     printf("write error to stdout\n");
             }*/
             //printf("%s\n",rbuf);
             if(write(STDOUT_FILENO,rbuf,nread) != nread)
                 printf("write error to stdout\n");
              

              //如果该输密码了，让输入不可见
              if (replycode == PASSWORD)
              {
                set_disp_mode(STDIN_FILENO,0);  
              }
              else{
                set_disp_mode(STDIN_FILENO,1);  
              }

             /*else 
                 printf("%d\n",-1);*/            
         }
    }
}




int checkSpeed(float speed,float timeuse){
  float sleepTime=0;
  //debug purpose only
  //printf("speed is %.2f MB/s\n",speed);
  //printf("timeuse is %.2f\n",timeuse);
  if (speed>SPEEDLIMIT){
    sleepTime = (1/SPEEDLIMIT)*timeuse-timeuse;
    //change from nano sec to sec
    sleepTime /= 10000000;
    sleepTime +=1;
    printf("sleepTime is %.2f s \n",sleepTime);
    sleep(sleepTime);
    return 1;
  }
  return 0;
}


/* download file from ftp server */
int ftp_get_limit(int sck,char *pDownloadFileName)
{
   int handle = open(pDownloadFileName,O_WRONLY | O_CREAT | O_TRUNC, S_IREAD| S_IWRITE);
   int nread;
   int loopCount = 0;
   int packageCount=0;
   int checkSpeedFlag = 0;
   printf("%d\n",handle);
   /*if(handle == -1) 
       return -1;*/
   struct timeval start, end;
   printf("downloading...\n");
   //checkSpeed
   gettimeofday( &start, NULL );
   for(;;)
   {  

      if(checkSpeedFlag == 1){
        //printf("Entering pause mode\n");
      }
      else{
        if((nread = recv(sck,rbuf1,MAXBUF,0)) < 0 )
       {  
          printf("receive error\n");
       }
       else if(nread == 0)
       {
          printf("over\n");
          break;
       } 
      }
       
       //Limiting speed function
       
       //printf("nread is %d\n",nread);//1024,760 
       packageCount += nread;
       loopCount += 1;
       // 时间约为1s,但是具体时间也会变化
       if(loopCount % 100 == 0)
       {
              gettimeofday( &end, NULL );
              float timeuse = 1000000 * ( end.tv_sec - start.tv_sec ) + end.tv_usec - start.tv_usec;
       
              //MB/s
              
              //debug purpose only
              //printf("current package count is %d  ",packageCount);
              

              float speed = packageCount / timeuse;
              checkSpeedFlag = checkSpeed(speed,timeuse);
              
              if (checkSpeedFlag == 1)
              { 
                printf("current speed is %.2f MB/s\n ",SPEEDLIMIT);
                if((nread = recv(sck,rbuf1,1,0)) < 0 )
                 {  
                    printf("receive error\n");
                 }
                 else if(nread == 0)
                 {
                    printf("over\n");
                    break;
                 } 
              }
              else{
                printf("current speed is %.2f MB/s\n " ,speed );  
              }
              


              gettimeofday( &start, NULL );
              packageCount =0;

            }

       //printf("%s\n",rbuf1);
       if(write(handle,rbuf1,nread) != nread)
           printf("receive error from server!");


      /*  Not print file content to console
       if(write(STDOUT_FILENO,rbuf1,nread) != nread)
           printf("receive error from server!");
      */
   }
       if(close(sck) < 0)
           printf("close error\n");
      return 0;
}

/* upload file to ftp server */
int ftp_put_limit(int sck,char *pUploadFileName_s)
{
   //int c_sock;
   int handle = open(pUploadFileName_s,O_RDWR);
   int nread;
   //error open
   if(handle == -1)
       return -1;
   //ftp_type(c_sock,"I");

   for(;;)
   {
       if((nread = read(handle,rbuf1,MAXBUF)) < 0)
       {
            printf("read error!");
       }
       else if(nread == 0)
          break;
       /*
       if(write(STDOUT_FILENO,rbuf1,nread) != nread)
            printf("send error!");
      */

       if(write(sck,rbuf1,nread) != nread)
            printf("send error!");
   }
   bzero(rbuf1,strlen(rbuf1));
   if(close(sck) < 0)
        printf("close error\n");
    return 0;
}


/* Read and write as command connection */
void cmd_tcp_limit(int sockfd)
{
    int maxfdp1,nread,nwrite,fd,replycode,tag=0,data_sock;
    int port;
    int newfilenameLen;
    char *pathname;
    fd_set rset;                
    FD_ZERO(&rset);                
    maxfdp1 = sockfd + 1;        /* check descriptors [0..sockfd] */

    for(;;)
    {
        //1.将  标准输入加入 可读文件描述符集合
         FD_SET(STDIN_FILENO,&rset);
        //2.将  命令套接字   加入可读文件描述符集合
         FD_SET(sockfd,&rset);

        //3.监听读事件
         if(select(maxfdp1,&rset,NULL,NULL,NULL)<0)
         {
             printf("select error\n");
         } 
         //4.判断标准输入是否有读事件
         if(FD_ISSET(STDIN_FILENO,&rset))
          // && replycode  != PASSWORD
         {
             //5.清空读缓冲区 和 写缓冲区
              bzero(wbuf,MAXBUF);          //zero
              bzero(rbuf1,MAXBUF);
              
              
              //set_disp_mode(STDIN_FILENO,1);  

              if((nread = read(STDIN_FILENO,rbuf1,MAXBUF)) <0)
                   printf("read error from stdin\n");
              nwrite = nread + 5;


            /* send username */
              if(replycode == USERNAME) 
              {
                  sprintf(wbuf,"USER %s",rbuf1);
              
                 if(write(sockfd,wbuf,nwrite) != nwrite)
                 {
                     printf("write error\n");
                 }
                 //printf("%s\n",wbuf);
                 //memset(rbuf1,0,sizeof(rbuf1));
                 //memset(wbuf,0,sizeof(wbuf));
                 //printf("1:%s\n",wbuf);
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
              if(replycode == 550 || replycode == LOGIN || replycode == CLOSEDATA || replycode == PATHNAME || replycode == ACTIONOK)
              {
                /* pwd -  print working directory */
                if(strncmp(rbuf1,"pwd",3) == 0)
                {   
                     //printf("%s\n",rbuf1);
                     sprintf(wbuf,"%s","PWD\n");
                     write(sockfd,wbuf,4);
                     continue; 
                 }
                 else if(strncmp(rbuf1,"quit",4) == 0)
                 {
                     sprintf(wbuf,"%s","QUIT\n");
                     write(sockfd,wbuf,5);
                     //close(sockfd);
                    if(close(sockfd) <0)
                       printf("close error\n");
                    printf("221 Goodbye.");
                    break;
                 }
                  /*************************************************************
                  // 5. code here: cd - change working directory/
                  *************************************************************/                 
                 else if(strncmp(rbuf1,"cd",2) == 0)
                 {
                     //sprintf(wbuf,"%s","PASV\n");
                     sscanf(rbuf1,"%s %s", tmp, dirname);
                     //printf("%s\n", dirname);
                     int dirnameLen= strlen(dirname);
                     //if not, the final character will be the \000
                     //dirname[dirnameLen] = '\n';
                     sprintf(wbuf,"CWD %s\n",dirname);
                     write(sockfd,wbuf,strlen(wbuf));
                     
                     //sprintf(wbuf1,"%s","CWD\n");
                     
                     continue;
                 }

                 // mkdir function
                 else if(strncmp(rbuf1,"mkdir",5) == 0 && strlen(rbuf1)!=5)
                 {
                     //sprintf(wbuf,"%s","PASV\n");
                     sscanf(rbuf1,"%s %s", tmp, dirname);
                     //printf("%s\n", dirname);
                     int dirnameLen= strlen(dirname);
                     //if not, the final character will be the \000
                     //dirname[dirnameLen] = '\n';
                     sprintf(wbuf,"MKD %s\n",dirname);
                     write(sockfd,wbuf,strlen(wbuf));
                     //sprintf(wbuf1,"%s","CWD\n");
                     
                     continue;
                 }

                 else if(strncmp(rbuf1,"delete",6) == 0 && strlen(rbuf1)!=5)
                 {
                     sscanf(rbuf1,"%s %s", tmp, filename);
                     //printf("%s\n", dirname);
                     int filenameLen= strlen(filename);
                     //if not, the final character will be the \000
                     //filename[filenameLen] = '\n';
                     sprintf(wbuf,"DELE %s\n",filename);
                     write(sockfd,wbuf,strlen(wbuf));
                     nwrite = 0;
                     //write(sockfd,wbuf1,nwrite);
                     //ftp_list(sockfd);
                     continue;   
                 }


                 /* ls - list files and directories*/
                 else if(strncmp(rbuf1,"ls",2) == 0)
                 {
                     tag = 2;            //显示文件 标识符
                     //printf("%s\n",rbuf1);
                     sprintf(wbuf,"%s","PASV\n");
                     //printf("%s\n",wbuf);
                     write(sockfd,wbuf,5);
                     //read
                     //sprintf(wbuf1,"%s","LIST -al\n");
                     nwrite = 0;
                     //write(sockfd,wbuf1,nwrite);
                     //ftp_list(sockfd);
                     continue;    
                 }
                  /*************************************************************
                  // 7. code here: get - get file from ftp server
                  *************************************************************/
                 else if(strncmp(rbuf1,"get",3) == 0 && strlen(rbuf1)!=3)
                 {
                     tag = 1;            //下载文件标识符

                     //被动传输模式    
                     sprintf(wbuf,"%s","PASV\n");                   
                     //printf("%s\n",s(rbuf1));
                     //char filename[100];
                     s(rbuf1,filename);
                     
                     //printf("%s\n",filename);
                     write(sockfd,wbuf,5);
                     continue;
                 }
                  /*************************************************************
                  // 8. code here: put -  put file upto ftp server
                  *************************************************************/                 
                 else if(strncmp(rbuf1,"put",3) == 0 && strlen(rbuf1)!=3)
                 {
                     tag = 3;            //上传文件标识符
                     sprintf(wbuf,"%s","PASV\n");

                    //把内容赋值给  读缓冲区
                     st(rbuf1,filename);
                     //printf("%s\n",filename);
                     write(sockfd,wbuf,5);
                     continue;
                 }

                 //binary mode and ascii mode
                else if(strncmp(rbuf1,"binary",6) == 0)
                 {
                     sprintf(wbuf,"TYPE %s","I\n");
                     write(sockfd,wbuf,7);
                     continue;
                 }
                 else if(strncmp(rbuf1,"ascii",5) == 0)
                 {
                     sprintf(wbuf,"TYPE %s","A\n");
                     write(sockfd,wbuf,7);
                     continue;
                 }

                 else if(strncmp(rbuf1,"rename",6) == 0 && strlen(rbuf1)!=6)
                 {
                     //sprintf(wbuf,"%s","PASV\n");
                     sscanf(rbuf1,"%s %s %s", tmp, filename,newfilename);
                     //printf("%s\n", dirname);
                     int filenameLen= strlen(filename);
                     newfilenameLen = strlen(newfilename);
                     //if not, the final character will be the \000
                     filename[filenameLen] = '\n';
                     newfilename[newfilenameLen] = '\n';
                     sprintf(wbuf,"RNFR %s",filename);
                     write(sockfd,wbuf,filenameLen+6);

                     

                     nwrite = 0;
                     //sprintf(wbuf1,"%s","CWD\n");
                     
                     continue;
                 }
                 else{
                  //write(sockfd,rbuf1,strlen(rbuf1));
                  //wbuf="Invalid input";
                 printf("Invalid input\n");
                 //保持被动传输模式    
                 tag=0;
                     sprintf(wbuf,"%s","PASV\n");                   
                     write(sockfd,wbuf,5);
                 
                  continue;
                 }




              } 
                    /*if(close(sockfd) <0)
                       printf("close error\n");*/
         }
         if(FD_ISSET(sockfd,&rset))
         {
         //9.清空读缓冲区 和 写缓冲区
             bzero(rbuf,strlen(rbuf));
         //10.读套接字中的内容

             if((nread = recv(sockfd,rbuf,MAXBUF,0)) <0)
                  printf("recv error\n");
             else if(nread == 0)
               break;

             //printf("%s",rbuf);

             if(strncmp(rbuf,"220",3) ==0 || strncmp(rbuf,"530",3)==0)
             {
                /*if(write(STDOUT_FILENO,rbuf,nread) != nread)
                    printf("write error to stdout\n");*/

                 //链接字符串


                 //printf("your name:");
                 //getpasswd(rbuf, 20);
                 strcat(rbuf,"your name:");
                
                 nread += 12;
                 replycode = USERNAME;
             }
             if(strncmp(rbuf,"331",3) == 0)
             {
                /*if(write(STDOUT_FILENO,rbuf,nread) != nread)
                    printf("write error to stdout\n")*/;
                
                strcat(rbuf,"your password:\n");
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

             // Ready to rename
             if(strncmp(rbuf,"350",3) == 0)
             {
                bzero(wbuf,strlen(wbuf));
                sprintf(wbuf,"RNTO %s",newfilename);
                write(sockfd,wbuf,newfilenameLen+6);
                replycode = 350;
             }
             
             /*if(strncmp(rbuf,"150",3) == 0)
             {
                if(write(STDOUT_FILENO,rbuf,nread) != nread)
                    printf("write error to stdout\n");
             }*/    
             //fprintf(stderr,"%d\n",1);
             if(strncmp(rbuf,"227",3) == 0)
             {
                //printf("%d\n",1);
                /*if(write(STDOUT_FILENO,rbuf,nread) != nread)
                   printf("write error to stdout\n");*/

                //获取服务器返回的 接收数据的端口，和地址
                int port1 = strtosrv(rbuf);
                printf("%d\n",port1);
                printf("%s\n",host);

                //创建新的传输数据的套接字?
                //1. 猜测 ====================应该是将 ssl 接口放在这里，用来传输数据
                data_sock = cliopen(host,port1);
        


                if(tag == 2)
                {
                   write(sockfd,"LIST\n",strlen("LIST\n"));
                   ftp_list(data_sock);
                   /*if(write(STDOUT_FILENO,rbuf,nread) != nread)
                       printf("write error to stdout\n");*/
                   
                }
                //else if(strncmp(rbuf1,"get",3) == 0)
                else if(tag == 1)
                {
                    //sprintf(wbuf,"%s","RETR\n");
                    //printf("%s\n",wbuf);
                    //int str = strlen(filename);
                    //printf("%d\n",str);
                    sprintf(wbuf,"RETR %s\n",filename);

                    //printf("%s\n",wbuf);
                    //int p = 5 + str + 1;

                    //命令套接字中写入  下载文件命令
                    write(sockfd,wbuf,strlen(wbuf));

                    //9.清空读缓冲区 和 写缓冲区
                    bzero(rbuf,strlen(rbuf));
                      //10.读套接字中的内容

                    if((nread = recv(sockfd,rbuf,MAXBUF,0)) <0)
                        printf("recv error\n");

                    //printf("%s",rbuf);
                    if(strncmp(rbuf,"550",3) == 0)
                    {
                      replycode = 550;
                    }
                    // 如果有这个文件，就下载
                    else{
                      ftp_get_limit(data_sock,filename);
                    }
                    //printf("%d\n",write(sockfd,wbuf,strlen(wbuf)));
                    //printf("%d\n",p);

                    //下载文件 
                    
                }
                else if(tag == 3)
                {
                    // 上传文件
                    sprintf(wbuf,"STOR %s\n",filename);
                    //printf("%s",wbuf);
                    int handle = open(filename,O_RDWR);
                    //printf("%s",wbuf);

                    //Handle file not exist error
                    if (handle != -1)
                    {
                      close(handle);
                      write(sockfd,wbuf,strlen(wbuf));
                      ftp_put(data_sock,filename);
                    }
                    else{
                      bzero(wbuf,strlen(wbuf));
                      printf("File not exist\n");
                      printf("%s",wbuf);
                    }
                       //int c_sock;
                     
                    
                    
                }

                nwrite = 0;
             }
             /*if(strncmp(rbuf,"150",3) == 0)
             {
                 if(write(STDOUT_FILENO,rbuf,nread) != nread)
                     printf("write error to stdout\n");
             }*/
             //printf("%s\n",rbuf);
             if(write(STDOUT_FILENO,rbuf,nread) != nread)
                 printf("write error to stdout\n");
              

              //如果该输密码了，让输入不可见
              if (replycode == PASSWORD)
              {
                set_disp_mode(STDIN_FILENO,0);  
              }
              else{
                set_disp_mode(STDIN_FILENO,1);  
              }

             /*else 
                 printf("%d\n",-1);*/            
         }
    }
}
