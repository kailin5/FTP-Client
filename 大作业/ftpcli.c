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


/* DefinE global variables */
char *rbuf,*rbuf1,*wbuf,*wbuf1;           /* pointer that is malloc'ed */
char filename[100]; 
char tmp[100];
char dirname[100];            
char *host;                            /* hostname or dotted-decimal string */
struct sockaddr_in servaddr;   


int mygetch();
int getpasswd(char *passwd, int size);
int set_disp_mode(int fd,int option);
int cliopen(char *host,int port);
int strtosrv(char *str);
void  ftp_list(int sockfd);
int ftp_get(int sck,char *pDownloadFileName);
int ftp_put(int sck,char *pUploadFileName_s);
void cmd_tcp(int sockfd);                    

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

int main(int argc,char *argv[])
{
    int fd;
    // Judge whether missing parameters
    if(0 != argc -2)
    {
        printf("%s\n","missing <hostname>");
        exit(0);
    }

    //define host and port
    host = argv[1];
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
    cmd_tcp(fd);
    exit(0);
}

int mygetch()
{
    struct termios oldt, newt;
    int ch;
    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);
    ch = getchar();
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    return ch;
}

// password to * method 
int getpasswd(char *passwd, int size)
{
    int c, n = 0;
    do
    {
        c = mygetch();
        if (c != '\n' && c != 'r' && c != 127)
        {
            passwd[n] = c;
            printf("*");
            n++;
        }
        else if ((c != '\n' | c != '\r') && c == 127)//判断是否是回车或则退格
        {
            if (n > 0)
            {
                n--;
                printf("\b \b");//输出退格
            }
        }
    }while (c != '\n' && c != '\r' && n < (size - 1));
    passwd[n] = '\0';//消除一个多余的回车
    return n;
}


/* Establish a TCP connection from client to server */
int cliopen(char *host,int port)
{   
    //control socket, in corresponding to data socket
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

/* download file from ftp server */
int ftp_get(int sck,char *pDownloadFileName)
{
   int handle = open(pDownloadFileName,O_WRONLY | O_CREAT | O_TRUNC, S_IREAD| S_IWRITE);
   int nread;
   printf("%d\n",handle);
   /*if(handle == -1) 
       return -1;*/

   
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
int ftp_put(int sck,char *pUploadFileName_s)
{
   //int c_sock;
   int handle = open(pUploadFileName_s,O_RDWR);
   int nread;
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
       if(write(STDOUT_FILENO,rbuf1,nread) != nread)
            printf("send error!");
       if(write(sck,rbuf1,nread) != nread)
            printf("send error!");
   }
   if(close(sck) < 0)
        printf("close error\n");
}


/* Read and write as command connection */
void cmd_tcp(int sockfd)
{
    int maxfdp1,nread,nwrite,fd,replycode,tag=0,data_sock;
    int port;
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

         /*
          //4.判断是否应该输入密码
         if(replycode  == PASSWORD )
         {
             //5.清空读缓冲区 和 写缓冲区
              bzero(wbuf,MAXBUF);          //zero
              bzero(rbuf1,MAXBUF);
              
              //getpasswd(rbuf1, 100);

               //formal input              
              if((nread = read(STDIN_FILENO,rbuf1,MAXBUF)) <0)
                   printf("read error from stdin\n");
              nwrite = nread + 5;
              

              sprintf(wbuf,"PASS %s",rbuf1);
                   if(write(sockfd,wbuf,nwrite) != nwrite)
                      printf("write error\n");
          }
          */  

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
                  // 5. code here: cd - change working directory/
                  *************************************************************/                 
                 if(strncmp(rbuf1,"cd",2) == 0)
                 {
                     //sprintf(wbuf,"%s","PASV\n");
                     sscanf(rbuf1,"%s %s", tmp, dirname);
                     //printf("%s\n", dirname);
                     int dirnameLen= strlen(dirname);
                     //if not, the final character will be the \000
                     dirname[dirnameLen] = '\n';
                     sprintf(wbuf,"CWD %s",dirname);
                     write(sockfd,wbuf,nread+1);
                     
                     //sprintf(wbuf1,"%s","CWD\n");
                     
                     continue;
                 }
                 /* ls - list files and directories*/
                 if(strncmp(rbuf1,"ls",2) == 0)
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

                 //to be added binary mode and ascii mode
                if(strncmp(rbuf1,"binary",6) == 0)
                 {
                     sprintf(wbuf,"TYPE %s","I\n");

                    //把内容赋值给  读缓冲区
                     st(rbuf1,filename);
                     printf("%s\n",filename);
                     write(sockfd,wbuf,7);
                     continue;
                 }
                 if(strncmp(rbuf1,"ascii",5) == 0)
                 {
                     sprintf(wbuf,"TYPE %s","A\n");

                    //把内容赋值给  读缓冲区
                     st(rbuf1,filename);
                     printf("%s\n",filename);
                     write(sockfd,wbuf,7);
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
        


//bzero(rbuf,sizeof(rbuf));
                //printf("%d\n",fd);
                //if(strncmp(rbuf1,"ls",2) == 0)
                if(tag == 2)
                {
                   write(sockfd,"list\n",strlen("list\n"));
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
                    printf("%s\n",wbuf);
                    //int p = 5 + str + 1;

                    //命令套接字中写入  下载文件命令
                    printf("%d\n",write(sockfd,wbuf,strlen(wbuf)));
                    //printf("%d\n",p);

                    //下载文件 
                    ftp_get(data_sock,filename);
                }
                else if(tag == 3)
                {

                    // 上传文件
                    sprintf(wbuf,"STOR %s\n",filename);
                    printf("%s\n",wbuf);
                    write(sockfd,wbuf,strlen(wbuf));
                    ftp_put(data_sock,filename);
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
