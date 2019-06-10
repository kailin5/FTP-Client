
#include <stdio.h>
#include <stdlib.h>
#include <netdb.h> 
#include <errno.h> 
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/fcntl.h>
#include <termios.h>  /* Used to not display password directly, instead using *****   */
#include<sys/time.h> 




#define ECHOFLAGS (ECHO | ECHOE | ECHOK | ECHONL)



#define MAXBUF             1024        
#define STDIN_FILENO     0            
#define STDOUT_FILENO     1            


#define USERNAME     220            
#define PASSWORD      331            
#define LOGIN           230            
#define PATHNAME      257            
#define CLOSEDATA     226       
#define ACTIONOK      250    

#define SPEEDLIMIT 0.01


char *rbuf,*rbuf1,*wbuf,*wbuf1;           
char filename[100];
char newfilename[100]; 
char tmp[100];
char dirname[100];            
char *host;                            
struct sockaddr_in servaddr;   

char activeCommand[27] = "PORT 10,128,234,192,50,113\n";
char activeAddress[100] = "10.128.234.192";
int binaryFlag = 1;



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


void remove_char_from_string(char c, char *str)
{
    int i=0;
    int len = strlen(str)+1;

    for(i=0; i<len; i++)
    {
        if(str[i] == c)
        {
            
            strncpy(&str[i],&str[i+1],len-i);
        }
    }
}


void add_char_from_string(char c, char *str)
{
    int i=0;
    int len = strlen(str)+1;

    char *tempStr;
    
    strncpy(&tempStr[0],&str[0],len);


    for(i=0; i<len; i++)
    {
        if(str[i] == '\n')
        {
            
            strncpy(&str[i+1],&tempStr[i],len-i);
            str[i] = '\r';
        }
    }
}


int main(int argc,char *argv[])
{
    int fd;
    int mode;
    
    if(0 != argc -3)
    {
        printf("%s\n","missing <hostname> or <mode>");
        exit(0);
    }

    
    host = argv[1];
    mode = atoi(argv[2]);
    int port = 21;
   

    /*****************************************************************
    
    
    *****************************************************************/
    rbuf = (char *)malloc(MAXBUF*sizeof(char));
    rbuf1 = (char *)malloc(MAXBUF*sizeof(char));
    wbuf = (char *)malloc(MAXBUF*sizeof(char));
    wbuf1 = (char *)malloc(MAXBUF*sizeof(char));

    
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
  
  return sock;

  
}


int cliopen(char *host,int port)
{   
    
    int control_sock;
    struct hostent *ht = NULL;

    control_sock = socket(AF_INET,SOCK_STREAM,0);
    if(control_sock < 0)
    {
       printf("socket error\n");
       return -1;
    }

    
    ht = gethostbyname(host);
    if(!ht)
    { 
        return -1;
    }
    
   
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


int s(char *str,char *s2)
{
    
     
    return sscanf(str," get %s",s2) == 1;
   
}


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
  
  EG:10,3,255,85,192,181  192*256+181 = 49333
  *************************************************************/
   int addr[6];
   
   sscanf(str,"%*[^(](%d,%d,%d,%d,%d,%d)",&addr[0],&addr[1],&addr[2],&addr[3],&addr[4],&addr[5]);
   
   bzero(host,strlen(host));
   sprintf(host,"%d.%d.%d.%d",addr[0],addr[1],addr[2],addr[3]);
   int port = addr[4]*256 + addr[5];
   return port;
}


void ftp_list(int sockfd)
{
    int nread;
    for( ; ; )
    {
        
        if((nread = recv(sockfd,rbuf1,MAXBUF,0)) < 0)
        {
            printf("recv error\n");
        }
        else if(nread == 0)
        {
            
            break;
        }
        if(write(STDOUT_FILENO,rbuf1,nread) != nread)
            printf("send error to stdout\n");
        
    }
    if(close(sockfd) < 0)
        printf("close error\n");
}




int ftp_get(int sck,char *pDownloadFileName)
{
   int handle = open(pDownloadFileName,O_WRONLY | O_CREAT | O_TRUNC, S_IREAD| S_IWRITE);
   int nread;
   printf("%d\n",handle);
   
   struct timeval start, end;
   
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

       
       if(binaryFlag == 0){
          remove_char_from_string('\r',rbuf1);
       }
       if(write(handle,rbuf1,nread) != nread)
           printf("receive error from server!");

      
         
   }
        bzero(rbuf1,strlen(rbuf1));
       if(close(sck) < 0)
           printf("close error\n");
      return 0;
}


int ftp_put(int sck,char *pUploadFileName_s)
{
   
   int handle = open(pUploadFileName_s,O_RDWR);
   int nread;
   
   if(handle == -1)
       return -1;
   

   for(;;)
   {
       if((nread = read(handle,rbuf1,MAXBUF)) < 0)
       {
            printf("read error!");
       }
       else if(nread == 0)
          break;
       
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



void cmd_tcp(int sockfd)
{
    int maxfdp1,nread,nwrite,fd,replycode,tag=0,data_sock;
    int port;
    int newfilenameLen;
    char *pathname;
    fd_set rset;                
    FD_ZERO(&rset);                
    maxfdp1 = sockfd + 1;        

    for(;;)
    {
        
         FD_SET(STDIN_FILENO,&rset);
        
         FD_SET(sockfd,&rset);

        
         if(select(maxfdp1,&rset,NULL,NULL,NULL)<0)
         {
             printf("select error\n");
         } 
         
         if(FD_ISSET(STDIN_FILENO,&rset))
          
         {
             
              bzero(wbuf,MAXBUF);          
              bzero(rbuf1,MAXBUF);
              
              
              

              if((nread = read(STDIN_FILENO,rbuf1,MAXBUF)) <0)
                   printf("read error from stdin\n");
              nwrite = nread + 5;


            
              if(replycode == USERNAME) 
              {
                  sprintf(wbuf,"USER %s",rbuf1);
              
                 if(write(sockfd,wbuf,nwrite) != nwrite)
                 {
                     printf("write error\n");
                 }
                 
                 
                 
                 
              }
             /*************************************************************
             
             *************************************************************/
              if(replycode == PASSWORD)
              {
                   
                   sprintf(wbuf,"PASS %s",rbuf1);
                   if(write(sockfd,wbuf,nwrite) != nwrite)
                      printf("write error\n");
                   
                   
                   
              }
              
               
              if(replycode == 550 || replycode == LOGIN || replycode == CLOSEDATA || replycode == PATHNAME || replycode == ACTIONOK)
              {
                
                if(strncmp(rbuf1,"pwd",3) == 0)
                {   
                     
                     sprintf(wbuf,"%s","PWD\n");
                     write(sockfd,wbuf,4);
                     continue; 
                 }
                 else if(strncmp(rbuf1,"quit",4) == 0)
                 {
                     sprintf(wbuf,"%s","QUIT\n");
                     write(sockfd,wbuf,5);
                     
                    if(close(sockfd) <0)
                       printf("close error\n");
                    printf("221 Goodbye.");
                    break;
                 }
                  /*************************************************************
                  
                  *************************************************************/                 
                 else if(strncmp(rbuf1,"cd",2) == 0 && strlen(rbuf1)!=3)
                 {
                     
                     sscanf(rbuf1,"%s %s", tmp, dirname);
                     
                     int dirnameLen= strlen(dirname);
                     
                     
                     sprintf(wbuf,"CWD %s\n",dirname);
                     write(sockfd,wbuf,strlen(wbuf));
                     
                     
                     
                     continue;
                 }

                 
                 else if(strncmp(rbuf1,"mkdir",5) == 0 && strlen(rbuf1)!=6)
                 {
                     
                     sscanf(rbuf1,"%s %s", tmp, dirname);
                     
                     int dirnameLen= strlen(dirname);
                     
                     
                     sprintf(wbuf,"MKD %s\n",dirname);
                     write(sockfd,wbuf,strlen(wbuf));
                     
                     
                     continue;
                 }

                 else if(strncmp(rbuf1,"delete",6) == 0 && strlen(rbuf1)!=7)
                 {
                     sscanf(rbuf1,"%s %s", tmp, filename);
                     
                     int filenameLen= strlen(filename);
                     
                     
                     sprintf(wbuf,"DELE %s\n",filename);
                     write(sockfd,wbuf,strlen(wbuf));
                     nwrite = 0;
                     
                     continue;   
                 }


                 
                 else if(strncmp(rbuf1,"ls",2) == 0)
                 {
                     tag = 2;            
                     
                     sprintf(wbuf,"%s","PASV\n");
                     
                     write(sockfd,wbuf,5);
                     
                     
                     nwrite = 0;
                     
                     
                     continue;    
                 }
                  /*************************************************************
                  
                  *************************************************************/
                 else if(strncmp(rbuf1,"get",3) == 0 && strlen(rbuf1)!=4)
                 {
                     tag = 1;            

                     
                     sprintf(wbuf,"%s","PASV\n");                   
                     
                     
                     s(rbuf1,filename);
                     
                     
                     write(sockfd,wbuf,5);
                     continue;
                 }
                  /*************************************************************
                  
                  *************************************************************/                 
                 else if(strncmp(rbuf1,"put",3) == 0 && strlen(rbuf1)!=4)
                 {
                     tag = 3;            
                     sprintf(wbuf,"%s","PASV\n");

                    
                     st(rbuf1,filename);
                     
                     write(sockfd,wbuf,5);
                     continue;
                 }

                 
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
                     
                     sscanf(rbuf1,"%s %s %s", tmp, filename,newfilename);
                     
                     
                     int filenameLen= strlen(filename);
                     newfilenameLen = strlen(newfilename);
                     
                     if (newfilenameLen == 0){
                      printf("Please type in new file name!\n");
                      replycode = 550;

                     }
                     else{
                         
                     filename[filenameLen] = '\n';
                     newfilename[newfilenameLen] = '\n';


                     sprintf(wbuf,"RNFR %s",filename);
                     write(sockfd,wbuf,filenameLen+6);

                     }
                     
                     

                     nwrite = 0;
                     
                     
                     continue;
                 }
                 else{
                  
                  
                 printf("This is an Invalid input!!!!!!\n");
                 
                 tag=0;
                 replycode = 550;
                     
                     
                 
                  continue;
                 }




              } 
                    
         }
         if(FD_ISSET(sockfd,&rset))
         {
         
             bzero(rbuf,strlen(rbuf));
         

             if((nread = recv(sockfd,rbuf,MAXBUF,0)) <0)
                  printf("recv error\n");
             else if(nread == 0)
               break;

             

             if(strncmp(rbuf,"220",3) ==0 || strncmp(rbuf,"530",3)==0)
             {
                

                 


                 
                 
                 strcat(rbuf,"your name:");
                
                 nread += 12;
                 replycode = USERNAME;
             }
             if(strncmp(rbuf,"331",3) == 0)
             {
                ;
                
                strcat(rbuf,"your password:\n");
                nread += 16;
                
                replycode = PASSWORD;
             }
             if(strncmp(rbuf,"230",3) == 0)
             {
                
                replycode = LOGIN;
             }
             if(strncmp(rbuf,"257",3) == 0)
             {
                
                replycode = PATHNAME;  
             }
             if(strncmp(rbuf,"226",3) == 0)
             {
                
                replycode = CLOSEDATA;
             }
             if(strncmp(rbuf,"250",3) == 0)
             {
                
                replycode = ACTIONOK;
             }
             if(strncmp(rbuf,"550",3) == 0)
             {
                replycode = 550;
             }

             
             if(strncmp(rbuf,"350",3) == 0)
             {
                bzero(wbuf,strlen(wbuf));
                sprintf(wbuf,"RNTO %s",newfilename);
                write(sockfd,wbuf,newfilenameLen+6);
                replycode = 350;
             }
             
                 
             
             if(strncmp(rbuf,"227",3) == 0)
             {
                
                

                
                int port1 = strtosrv(rbuf);
                
                
                
                
                printf("Retrieving Data...\n");

                
                
                data_sock = cliopen(host,port1);
        

                if(tag == 2)
                {
                   write(sockfd,"LIST\n",strlen("LIST\n"));
                   ftp_list(data_sock);
                   
                   
                }
                
                else if(tag == 1)
                {   

                    
                    
                    
                    
                    

                    



                    sprintf(wbuf,"RETR %s\n",filename);

                    
                    

                    
                    write(sockfd,wbuf,strlen(wbuf));

                    
                    bzero(rbuf,strlen(rbuf));
                      

                    if((nread = recv(sockfd,rbuf,MAXBUF,0)) <0)
                        printf("recv error\n");

                    
                    if(strncmp(rbuf,"550",3) == 0)
                    {
                      replycode = 550;
                    }
                    
                    else{
                      ftp_get(data_sock,filename);
                    }
                    
                    

                     
                    
                }
                else if(tag == 3)
                {
                    
                    sprintf(wbuf,"STOR %s\n",filename);
                    
                    int handle = open(filename,O_RDWR);
                    

                    
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
                       
                     
                    
                    
                }
                nwrite = 0;
             }
             
             
             if(write(STDOUT_FILENO,rbuf,nread) != nread)
                 printf("write error to stdout\n");
              

              
              if (replycode == PASSWORD)
              {
                set_disp_mode(STDIN_FILENO,0);  
              }
              else{
                set_disp_mode(STDIN_FILENO,1);  
              }

                         
         }
    }
}




void ftp_list_active(int sockfd)
{
    int nread;
    struct sockaddr_in clientAddr;
    unsigned int cliAddrLen;
    
    
    int newsockfd = 0;
        cliAddrLen = sizeof(clientAddr);
        if((newsockfd = accept(sockfd, (struct sockaddr *) &clientAddr, &cliAddrLen)) < 0)
        {
          printf("accept() failed.\n");
        exit(1);
      }

    for( ; ; )
    {   

        
        if((nread = recv(newsockfd,rbuf1,MAXBUF,0)) < 0)
        {
            printf("recv error\n");
        }
        else if(nread == 0)
        {
            
            break;
        }
        if(write(STDOUT_FILENO,rbuf1,nread) != nread)
            printf("send error to stdout\n");
        
          printf("%d",nread);
    }
    if(close(sockfd) < 0)
        printf("close error\n");
    if(close(newsockfd) < 0)
        printf("close error\n");
}




int ftp_get_active(int sck,char *pDownloadFileName)
{
   int handle = open(pDownloadFileName,O_WRONLY | O_CREAT | O_TRUNC, S_IREAD| S_IWRITE);
   int nread;
   
   
   
   struct sockaddr_in clientAddr;
    unsigned int cliAddrLen;
    
    
    int newsockfd = 0;
        cliAddrLen = sizeof(clientAddr);
        if((newsockfd = accept(sck, (struct sockaddr *) &clientAddr, &cliAddrLen)) < 0)
        {
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
       
       if(write(handle,rbuf1,nread) != nread)
           printf("receive error from server!");

      
   }
       if(close(sck) < 0)
           printf("close error\n");
        if(close(newsockfd) < 0)
        printf("close error\n");
      return 0;
}


int ftp_put_active(int sck,char *pUploadFileName_s)
{
   
   int handle = open(pUploadFileName_s,O_RDWR);
   int nread;
   
   if(handle == -1)
       return -1;
   
    struct sockaddr_in clientAddr;
    unsigned int cliAddrLen;
    
    
    int newsockfd = 0;
        cliAddrLen = sizeof(clientAddr);
        if((newsockfd = accept(sck, (struct sockaddr *) &clientAddr, &cliAddrLen)) < 0)
        {
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



void cmd_tcp_active(int sockfd)
{
    int maxfdp1,nread,nwrite,fd,replycode,tag=0,data_sock;
    int port;
    int newfilenameLen;
    char *pathname;
    fd_set rset;                
    FD_ZERO(&rset);                
    maxfdp1 = sockfd + 1;        

    for(;;)
    {
        
         FD_SET(STDIN_FILENO,&rset);
        
         FD_SET(sockfd,&rset);

        
         if(select(maxfdp1,&rset,NULL,NULL,NULL)<0)
         {
             printf("select error\n");
         } 
         
         if(FD_ISSET(STDIN_FILENO,&rset))
          
         {
             
              bzero(wbuf,MAXBUF);          
              bzero(rbuf1,MAXBUF);
              
              
              

              if((nread = read(STDIN_FILENO,rbuf1,MAXBUF)) <0)
                   printf("read error from stdin\n");
              nwrite = nread + 5;


            
              if(replycode == USERNAME) 
              {
                  sprintf(wbuf,"USER %s",rbuf1);
              
                 if(write(sockfd,wbuf,nwrite) != nwrite)
                 {
                     printf("write error\n");
                 }
                 
                 
                 
                 
              }
             /*************************************************************
             
             *************************************************************/
              if(replycode == PASSWORD)
              {
                   
                   sprintf(wbuf,"PASS %s",rbuf1);
                   if(write(sockfd,wbuf,nwrite) != nwrite)
                      printf("write error\n");
                   
                   
                   
              }
              
               
              if(replycode == 550 || replycode == 553 || replycode == LOGIN || replycode == CLOSEDATA || replycode == PATHNAME || replycode == ACTIONOK)
              {
                
                if(strncmp(rbuf1,"pwd",3) == 0)
                {   
                     
                     sprintf(wbuf,"%s","PWD\n");
                     write(sockfd,wbuf,4);
                     continue; 
                 }
                 else if(strncmp(rbuf1,"quit",4) == 0)
                 {
                     sprintf(wbuf,"%s","QUIT\n");
                     write(sockfd,wbuf,5);
                     
                    if(close(sockfd) <0)
                       printf("close error\n");
                    printf("221 Goodbye.");
                    break;
                 }
                  /*************************************************************
                  
                  *************************************************************/                 
                 else if(strncmp(rbuf1,"cd",2) == 0)
                 {
                     sscanf(rbuf1,"%s %s", tmp, dirname);
                     
                     int dirnameLen= strlen(dirname);
                     
                     
                     sprintf(wbuf,"CWD %s\n",dirname);
                     write(sockfd,wbuf,strlen(wbuf));
                     
                     
                     
                     continue;
                 }

                 
                 else if(strncmp(rbuf1,"mkdir",5) == 0)
                 {
                     sscanf(rbuf1,"%s %s", tmp, dirname);
                     
                     int dirnameLen= strlen(dirname);
                     
                     
                     sprintf(wbuf,"MKD %s\n",dirname);
                     write(sockfd,wbuf,strlen(wbuf));
                     
                     
                     continue;
                 }

                 else if(strncmp(rbuf1,"delete",6) == 0)
                 {
                     tag = 4; 
                     
                     sprintf(wbuf,"%s",activeCommand);
                     
                     write(sockfd,wbuf,strlen(activeCommand));
                     
                     
                     nwrite = 0;
                     
                     
                     continue;   
                 }


                 
                 else if(strncmp(rbuf1,"ls",2) == 0)
                 {
                     tag = 2;            
                     
                     sprintf(wbuf,"%s",activeCommand);
                     
                     write(sockfd,wbuf,strlen(activeCommand));
                     
                     
                     nwrite = 0;
                     
                     
                     continue;    
                 }
                  /*************************************************************
                  
                  *************************************************************/
                 else if(strncmp(rbuf1,"get",3) == 0)
                 {
                     tag = 1;            

                     
                     sprintf(wbuf,"%s",activeCommand);                   
                     
                     
                     s(rbuf1,filename);
                     
                     
                     write(sockfd,wbuf,strlen(activeCommand));
                     continue;
                 }
                  /*************************************************************
                  
                  *************************************************************/                 
                 else if(strncmp(rbuf1,"put",3) == 0)
                 {
                     tag = 3;            
                     sprintf(wbuf,"%s",activeCommand);

                    
                     st(rbuf1,filename);
                     
                     write(sockfd,wbuf,strlen(activeCommand));
                     continue;
                 }

                 
                else if(strncmp(rbuf1,"binary",6) == 0)
                 {
                     sprintf(wbuf,"TYPE %s","I\n");

                    
                     st(rbuf1,filename);
                     printf("%s\n",filename);
                     write(sockfd,wbuf,7);
                     continue;
                 }
                 else if(strncmp(rbuf1,"ascii",5) == 0)
                 {
                     sprintf(wbuf,"TYPE %s","A\n");

                    
                     st(rbuf1,filename);
                     printf("%s\n",filename);
                     write(sockfd,wbuf,7);
                     continue;
                 }

                 else if(strncmp(rbuf1,"rename",6) == 0)
                 {
                     
                     sscanf(rbuf1,"%s %s %s", tmp, filename,newfilename);
                     
                     int filenameLen= strlen(filename);
                     newfilenameLen = strlen(newfilename);
                     
                     filename[filenameLen] = '\n';
                     newfilename[newfilenameLen] = '\n';
                     sprintf(wbuf,"RNFR %s",filename);
                     write(sockfd,wbuf,filenameLen+6);

                     

                     nwrite = 0;
                     
                     
                     continue;
                 }
                 else{
                  write(sockfd,rbuf1,strlen(rbuf1));
                  continue;
                 }




              } 
                    
         }
         if(FD_ISSET(sockfd,&rset))
         {
         
             bzero(rbuf,strlen(rbuf));
         

             if((nread = recv(sockfd,rbuf,MAXBUF,0)) <0)
                  printf("recv error\n");
             else if(nread == 0)
               break;

             

             if(strncmp(rbuf,"220",3) ==0 || strncmp(rbuf,"530",3)==0)
             {
                

                 


                 
                 
                 strcat(rbuf,"your name:");
                
                 nread += 12;
                 replycode = USERNAME;
             }
             if(strncmp(rbuf,"331",3) == 0)
             {
                ;
                
                strcat(rbuf,"your password:\n");
                nread += 16;
                
                replycode = PASSWORD;
             }
             if(strncmp(rbuf,"230",3) == 0)
             {
                
                replycode = LOGIN;
             }
             if(strncmp(rbuf,"257",3) == 0)
             {
                
                replycode = PATHNAME;  
             }
             if(strncmp(rbuf,"226",3) == 0)
             {
                
                replycode = CLOSEDATA;
             }
             if(strncmp(rbuf,"250",3) == 0)
             {
                
                replycode = ACTIONOK;
             }
             if(strncmp(rbuf,"550",3) == 0)
             {
                replycode = 550;
             }
             
             if(strncmp(rbuf,"553",3) == 0)
             {
                replycode = 553;
             }

             
             if(strncmp(rbuf,"350",3) == 0)
             {
                bzero(wbuf,strlen(wbuf));
                sprintf(wbuf,"RNTO %s",newfilename);
                write(sockfd,wbuf,newfilenameLen+6);
                replycode = 350;
             }
             
                 
             
             if(strncmp(rbuf,"200 PORT",8) == 0)
             {
                
                

                
                int port1 = 12913;
                printf("%d\n",port1);
                printf("%s\n",host);

                
                
                data_sock = dataSock(host,port1);
        


                if(tag == 2)
                {
                   write(sockfd,"LIST\n",strlen("LIST\n"));
                   ftp_list_active(data_sock);
                   
                   close(data_sock);
                }
                
                else if(tag == 1)
                {     
                  
                    
                    
                    
                    
                    sprintf(wbuf,"RETR %s\n",filename);

                    
                    

                    
                    write(sockfd,wbuf,strlen(wbuf));

                    
                    bzero(rbuf,strlen(rbuf));
                      

                    if((nread = recv(sockfd,rbuf,MAXBUF,0)) <0)
                        {
                          printf("recv error\n");
                          exit(1);
                        }

                    
                    if(strncmp(rbuf,"550",3) == 0)
                    {
                      replycode = 550;
                    }
                    
                    else{
                      ftp_get_active(data_sock,filename);
                    }
                    
                    

                    close(data_sock);
                    
                }
                else if(tag == 3)
                {
                    
                    sprintf(wbuf,"STOR %s\n",filename);
                    
                    int handle = open(filename,O_RDWR);
                    

                    
                    if (handle != -1)
                    {
                      close(handle);
                      write(sockfd,wbuf,strlen(wbuf));



                        
                      bzero(rbuf,strlen(rbuf));
                        

                      if((nread = recv(sockfd,rbuf,MAXBUF,0)) <0)
                          {
                            printf("recv error\n");
                            exit(1);
                          }

                      
                      if(strncmp(rbuf,"553",3) == 0)
                      {
                        replycode = 553;
                      }
                      
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
                       
                     
                    
                    close(data_sock);
                }
                else if (tag == 4){
                     sscanf(rbuf1,"%s %s", tmp, filename);
                     
                     int filenameLen= strlen(filename);
                     
                     
                     sprintf(wbuf,"DELE %s\n",filename);
                     write(sockfd,wbuf,strlen(wbuf));
                     
                     close(data_sock);
                     continue;
                }
                nwrite = 0;     
             }
             
             
             if(write(STDOUT_FILENO,rbuf,nread) != nread)
                 printf("write error to stdout\n");
              

              
              if (replycode == PASSWORD)
              {
                set_disp_mode(STDIN_FILENO,0);  
              }
              else{
                set_disp_mode(STDIN_FILENO,1);  
              }

                         
         }
    }
}






int ftp_get_resume(int sck,char *pDownloadFileName)
{
   int handle = open(pDownloadFileName,O_WRONLY | O_CREAT | O_TRUNC, S_IREAD| S_IWRITE);
   int nread;
   printf("%d\n",handle);
   
   struct timeval start, end;
   
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

       if(write(handle,rbuf1,nread) != nread)
           printf("receive error from server!");

      
         
   }
        bzero(rbuf1,strlen(rbuf1));
       if(close(sck) < 0)
           printf("close error\n");
      return 0;
}


int ftp_resume(int sck,char *pDownloadFileName)
{
   int handle = open(pDownloadFileName,O_RDWR | O_APPEND);
   int nread;
   printf("%d\n",handle);
   
    struct timeval start, end;
    
    printf("Now file is resuming downloading...\n"); 
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
       if(write(handle,rbuf1,nread) != nread)
           printf("receive error from server!\n");

      
         
   }
        bzero(rbuf1,strlen(rbuf1));
       if(close(sck) < 0)
           printf("close error\n");
      return 0;
}


int ftp_put_resume(int sck,char *pUploadFileName_s)
{
   
   int handle = open(pUploadFileName_s,O_RDWR);
   int nread;
   
   if(handle == -1)
       return -1;
   

   for(;;)
   {
       if((nread = read(handle,rbuf1,MAXBUF)) < 0)
       {
            printf("read error!");
       }
       else if(nread == 0)
          break;
       
       if(write(sck,rbuf1,nread) != nread)
            printf("send error!");
   }
   bzero(rbuf1,strlen(rbuf1));
   if(close(sck) < 0)
        printf("close error\n");
    return 0;
}



void cmd_tcp_resume(int sockfd)
{
    int maxfdp1,nread,nwrite,fd,replycode,tag=0,data_sock;
    int port;
    int newfilenameLen;
    char *pathname;
    fd_set rset;                
    FD_ZERO(&rset);                
    maxfdp1 = sockfd + 1;        

    for(;;)
    {
        
         FD_SET(STDIN_FILENO,&rset);
        
         FD_SET(sockfd,&rset);

        
         if(select(maxfdp1,&rset,NULL,NULL,NULL)<0)
         {
             printf("select error\n");
         } 
         
         if(FD_ISSET(STDIN_FILENO,&rset))
          
         {
             
              bzero(wbuf,MAXBUF);          
              bzero(rbuf1,MAXBUF);
              
              
              

              if((nread = read(STDIN_FILENO,rbuf1,MAXBUF)) <0)
                   printf("read error from stdin\n");
              nwrite = nread + 5;


            
              if(replycode == USERNAME) 
              {
                  sprintf(wbuf,"USER %s",rbuf1);
              
                 if(write(sockfd,wbuf,nwrite) != nwrite)
                 {
                     printf("write error\n");
                 }
                 
                 
                 
                 
              }
             /*************************************************************
             
             *************************************************************/
              if(replycode == PASSWORD)
              {
                   
                   sprintf(wbuf,"PASS %s",rbuf1);
                   if(write(sockfd,wbuf,nwrite) != nwrite)
                      printf("write error\n");
                   
                   
                   
              }
              
               
              if(replycode == 550 || replycode == LOGIN || replycode == CLOSEDATA || replycode == PATHNAME || replycode == ACTIONOK)
              {
                
                if(strncmp(rbuf1,"pwd",3) == 0)
                {   
                     
                     sprintf(wbuf,"%s","PWD\n");
                     write(sockfd,wbuf,4);
                     continue; 
                 }
                 else if(strncmp(rbuf1,"quit",4) == 0)
                 {
                     sprintf(wbuf,"%s","QUIT\n");
                     write(sockfd,wbuf,5);
                     
                    if(close(sockfd) <0)
                       printf("close error\n");
                    printf("221 Goodbye.");
                    break;
                 }
                  /*************************************************************
                  
                  *************************************************************/                 
                 else if(strncmp(rbuf1,"cd",2) == 0)
                 {
                     
                     sscanf(rbuf1,"%s %s", tmp, dirname);
                     
                     int dirnameLen= strlen(dirname);
                     
                     
                     sprintf(wbuf,"CWD %s\n",dirname);
                     write(sockfd,wbuf,strlen(wbuf));
                     
                     
                     
                     continue;
                 }

                 
                 else if(strncmp(rbuf1,"mkdir",5) == 0 && strlen(rbuf1)!=5)
                 {
                     
                     sscanf(rbuf1,"%s %s", tmp, dirname);
                     
                     int dirnameLen= strlen(dirname);
                     
                     
                     sprintf(wbuf,"MKD %s\n",dirname);
                     write(sockfd,wbuf,strlen(wbuf));
                     
                     
                     continue;
                 }

                 else if(strncmp(rbuf1,"delete",6) == 0 && strlen(rbuf1)!=5)
                 {
                     sscanf(rbuf1,"%s %s", tmp, filename);
                     
                     int filenameLen= strlen(filename);
                     
                     
                     sprintf(wbuf,"DELE %s\n",filename);
                     write(sockfd,wbuf,strlen(wbuf));
                     nwrite = 0;
                     
                     continue;   
                 }


                 
                 else if(strncmp(rbuf1,"ls",2) == 0)
                 {
                     tag = 2;            
                     
                     sprintf(wbuf,"%s","PASV\n");
                     
                     write(sockfd,wbuf,5);
                     
                     
                     nwrite = 0;
                     
                     
                     continue;    
                 }
                  /*************************************************************
                  
                  *************************************************************/
                 else if(strncmp(rbuf1,"get",3) == 0 && strlen(rbuf1)!=3)
                 {
                     tag = 1;            

                     
                     sprintf(wbuf,"%s","PASV\n");                   
                     
                     
                     s(rbuf1,filename);
                     
                     
                     write(sockfd,wbuf,5);
                     continue;
                 }
                  /*************************************************************
                  
                  *************************************************************/                 
                 else if(strncmp(rbuf1,"put",3) == 0 && strlen(rbuf1)!=3)
                 {
                     tag = 3;            
                     sprintf(wbuf,"%s","PASV\n");

                    
                     st(rbuf1,filename);
                     
                     write(sockfd,wbuf,5);
                     continue;
                 }

                 
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
                     
                     sscanf(rbuf1,"%s %s %s", tmp, filename,newfilename);
                     
                     int filenameLen= strlen(filename);
                     newfilenameLen = strlen(newfilename);
                     
                     filename[filenameLen] = '\n';
                     newfilename[newfilenameLen] = '\n';
                     sprintf(wbuf,"RNFR %s",filename);
                     write(sockfd,wbuf,filenameLen+6);

                     

                     nwrite = 0;
                     
                     
                     continue;
                 }
                 else{
                  
                  
                 printf("Invalid input\n");
                 
                 tag=0;
                     sprintf(wbuf,"%s","PASV\n");                   
                     write(sockfd,wbuf,5);
                 
                  continue;
                 }




              } 
                    
         }
         if(FD_ISSET(sockfd,&rset))
         {
         
             bzero(rbuf,strlen(rbuf));
         

             if((nread = recv(sockfd,rbuf,MAXBUF,0)) <0)
                  printf("recv error\n");
             else if(nread == 0)
               break;

             

             if(strncmp(rbuf,"220",3) ==0 || strncmp(rbuf,"530",3)==0)
             {
                

                 


                 
                 
                 strcat(rbuf,"your name:");
                
                 nread += 12;
                 replycode = USERNAME;
             }
             if(strncmp(rbuf,"331",3) == 0)
             {
                ;
                
                strcat(rbuf,"your password:\n");
                nread += 16;
                
                replycode = PASSWORD;
             }
             if(strncmp(rbuf,"230",3) == 0)
             {
                
                replycode = LOGIN;
             }
             if(strncmp(rbuf,"257",3) == 0)
             {
                
                replycode = PATHNAME;  
             }
             if(strncmp(rbuf,"226",3) == 0)
             {
                
                replycode = CLOSEDATA;
             }
             if(strncmp(rbuf,"250",3) == 0)
             {
                
                replycode = ACTIONOK;
             }
             if(strncmp(rbuf,"550",3) == 0)
             {
                replycode = 550;
             }

             
             if(strncmp(rbuf,"350",3) == 0)
             {
                bzero(wbuf,strlen(wbuf));
                sprintf(wbuf,"RNTO %s",newfilename);
                write(sockfd,wbuf,newfilenameLen+6);
                replycode = 350;
             }
             
                 
             
             if(strncmp(rbuf,"227",3) == 0)
             {
                
                

                
                int port1 = strtosrv(rbuf);
                printf("%d\n",port1);
                printf("%s\n",host);
                printf("Retrieving Data...\n");

                
                
                data_sock = cliopen(host,port1);
        

                if(tag == 2)
                {
                   write(sockfd,"LIST\n",strlen("LIST\n"));
                   ftp_list(data_sock);
                   
                   
                }
                
                else if(tag == 1)
                {   

                    
                    
                    
                    
                    

                    
                    FILE * pFile;
                    long size;
                    pFile = fopen (filename,"rb");
                    if (pFile==NULL)
                    {  
                       sprintf(wbuf,"RETR %s\n",filename);
                      
                      

                      
                      write(sockfd,wbuf,strlen(wbuf));
                      
                      bzero(rbuf,strlen(rbuf));
                        

                      if((nread = recv(sockfd,rbuf,MAXBUF,0)) <0)
                          printf("recv error\n");
                      
                      if(strncmp(rbuf,"550",3) == 0)
                        {
                          replycode = 550;
                        }
                      
                        else
                        {
                          ftp_get_resume(data_sock,filename);
                        }
                      }
                    else
                    {
                         fseek (pFile, 0, SEEK_END);   
                         size=ftell (pFile); 
                         fclose (pFile);
                         printf ("Size of this file: %ld bytes.\n",size);


                          sprintf(wbuf,"REST %ld\n",size);
                          write(sockfd,wbuf,strlen(wbuf));

                          
                          bzero(rbuf,strlen(rbuf));
                            

                          if((nread = recv(sockfd,rbuf,MAXBUF,0)) <0)
                              printf("recv error\n");


                          
                          if(strncmp(rbuf,"350",3) == 0)
                          {
                            
                          sprintf(wbuf,"TYPE I\n");
                          write(sockfd,wbuf,strlen(wbuf));

                          
                          bzero(rbuf,strlen(rbuf));
                            

                          if((nread = recv(sockfd,rbuf,MAXBUF,0)) <0)
                              printf("recv error\n");
                        
                            sprintf(wbuf,"RETR %s\n",filename);

                            
                            write(sockfd,wbuf,strlen(wbuf));

                            
                            bzero(rbuf,strlen(rbuf));
                              

                            if((nread = recv(sockfd,rbuf,MAXBUF,0)) <0)
                                printf("recv error\n");

                            
                            if(strncmp(rbuf,"550",3) == 0)
                              {
                                replycode = 550;
                              }
                            
                              else
                              {
                                ftp_resume(data_sock,filename);
                              }
                          }
                    }




                    
                    
                    

                     
                    
                }
                else if(tag == 3)
                {
                    
                    sprintf(wbuf,"STOR %s\n",filename);
                    
                    int handle = open(filename,O_RDWR);
                    

                    
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
                       
                     
                    
                    
                }
                nwrite = 0;
             }
             
             
             if(write(STDOUT_FILENO,rbuf,nread) != nread)
                 printf("write error to stdout\n");
              

              
              if (replycode == PASSWORD)
              {
                set_disp_mode(STDIN_FILENO,0);  
              }
              else{
                set_disp_mode(STDIN_FILENO,1);  
              }

                         
         }
    }
}




int checkSpeed(float speed,float timeuse){
  float sleepTime=0;
  
  
  
  if (speed>SPEEDLIMIT){
    sleepTime = (1/SPEEDLIMIT)*timeuse-timeuse;
    
    sleepTime /= 10000000;
    sleepTime +=1;
    printf("sleepTime is %.2f s \n",sleepTime);
    sleep(sleepTime);
    return 1;
  }
  return 0;
}



int ftp_get_limit(int sck,char *pDownloadFileName)
{
   int handle = open(pDownloadFileName,O_WRONLY | O_CREAT | O_TRUNC, S_IREAD| S_IWRITE);
   int nread;
   int loopCount = 0;
   int packageCount=0;
   int checkSpeedFlag = 0;
   printf("%d\n",handle);
   
   struct timeval start, end;
   printf("downloading...\n");
   
   gettimeofday( &start, NULL );
   for(;;)
   {  

      if(checkSpeedFlag == 1){
        
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
       
       
       
       
       packageCount += nread;
       loopCount += 1;
       
       if(loopCount % 100 == 0)
       {
              gettimeofday( &end, NULL );
              float timeuse = 1000000 * ( end.tv_sec - start.tv_sec ) + end.tv_usec - start.tv_usec;
       
              
              
              
              
              

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

       
       if(write(handle,rbuf1,nread) != nread)
           printf("receive error from server!");


      
   }
       if(close(sck) < 0)
           printf("close error\n");
      return 0;
}


int ftp_put_limit(int sck,char *pUploadFileName_s)
{
   
   int handle = open(pUploadFileName_s,O_RDWR);
   int nread;
   
   if(handle == -1)
       return -1;
   

   for(;;)
   {
       if((nread = read(handle,rbuf1,MAXBUF)) < 0)
       {
            printf("read error!");
       }
       else if(nread == 0)
          break;
       

       if(write(sck,rbuf1,nread) != nread)
            printf("send error!");
   }
   bzero(rbuf1,strlen(rbuf1));
   if(close(sck) < 0)
        printf("close error\n");
    return 0;
}



void cmd_tcp_limit(int sockfd)
{
    int maxfdp1,nread,nwrite,fd,replycode,tag=0,data_sock;
    int port;
    int newfilenameLen;
    char *pathname;
    fd_set rset;                
    FD_ZERO(&rset);                
    maxfdp1 = sockfd + 1;        

    for(;;)
    {
        
         FD_SET(STDIN_FILENO,&rset);
        
         FD_SET(sockfd,&rset);

        
         if(select(maxfdp1,&rset,NULL,NULL,NULL)<0)
         {
             printf("select error\n");
         } 
         
         if(FD_ISSET(STDIN_FILENO,&rset))
          
         {
             
              bzero(wbuf,MAXBUF);          
              bzero(rbuf1,MAXBUF);
              
              
              

              if((nread = read(STDIN_FILENO,rbuf1,MAXBUF)) <0)
                   printf("read error from stdin\n");
              nwrite = nread + 5;


            
              if(replycode == USERNAME) 
              {
                  sprintf(wbuf,"USER %s",rbuf1);
              
                 if(write(sockfd,wbuf,nwrite) != nwrite)
                 {
                     printf("write error\n");
                 }
                 
                 
                 
                 
              }
             /*************************************************************
             
             *************************************************************/
              if(replycode == PASSWORD)
              {
                   
                   sprintf(wbuf,"PASS %s",rbuf1);
                   if(write(sockfd,wbuf,nwrite) != nwrite)
                      printf("write error\n");
                   
                   
                   
              }
              
               
              if(replycode == 550 || replycode == LOGIN || replycode == CLOSEDATA || replycode == PATHNAME || replycode == ACTIONOK)
              {
                
                if(strncmp(rbuf1,"pwd",3) == 0)
                {   
                     
                     sprintf(wbuf,"%s","PWD\n");
                     write(sockfd,wbuf,4);
                     continue; 
                 }
                 else if(strncmp(rbuf1,"quit",4) == 0)
                 {
                     sprintf(wbuf,"%s","QUIT\n");
                     write(sockfd,wbuf,5);
                     
                    if(close(sockfd) <0)
                       printf("close error\n");
                    printf("221 Goodbye.");
                    break;
                 }
                  /*************************************************************
                  
                  *************************************************************/                 
                 else if(strncmp(rbuf1,"cd",2) == 0)
                 {
                     
                     sscanf(rbuf1,"%s %s", tmp, dirname);
                     
                     int dirnameLen= strlen(dirname);
                     
                     
                     sprintf(wbuf,"CWD %s\n",dirname);
                     write(sockfd,wbuf,strlen(wbuf));
                     
                     
                     
                     continue;
                 }

                 
                 else if(strncmp(rbuf1,"mkdir",5) == 0 && strlen(rbuf1)!=5)
                 {
                     
                     sscanf(rbuf1,"%s %s", tmp, dirname);
                     
                     int dirnameLen= strlen(dirname);
                     
                     
                     sprintf(wbuf,"MKD %s\n",dirname);
                     write(sockfd,wbuf,strlen(wbuf));
                     
                     
                     continue;
                 }

                 else if(strncmp(rbuf1,"delete",6) == 0 && strlen(rbuf1)!=5)
                 {
                     sscanf(rbuf1,"%s %s", tmp, filename);
                     
                     int filenameLen= strlen(filename);
                     
                     
                     sprintf(wbuf,"DELE %s\n",filename);
                     write(sockfd,wbuf,strlen(wbuf));
                     nwrite = 0;
                     
                     
                     continue;   
                 }


                 
                 else if(strncmp(rbuf1,"ls",2) == 0)
                 {
                     tag = 2;            
                     
                     sprintf(wbuf,"%s","PASV\n");
                     
                     write(sockfd,wbuf,5);
                     
                     
                     nwrite = 0;
                     
                     
                     continue;    
                 }
                  /*************************************************************
                  
                  *************************************************************/
                 else if(strncmp(rbuf1,"get",3) == 0 && strlen(rbuf1)!=3)
                 {
                     tag = 1;            

                     
                     sprintf(wbuf,"%s","PASV\n");                   
                     
                     
                     s(rbuf1,filename);
                     
                     
                     write(sockfd,wbuf,5);
                     continue;
                 }
                  /*************************************************************
                  
                  *************************************************************/                 
                 else if(strncmp(rbuf1,"put",3) == 0 && strlen(rbuf1)!=3)
                 {
                     tag = 3;            
                     sprintf(wbuf,"%s","PASV\n");

                    
                     st(rbuf1,filename);
                     
                     write(sockfd,wbuf,5);
                     continue;
                 }

                 
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
                     
                     sscanf(rbuf1,"%s %s %s", tmp, filename,newfilename);
                     
                     int filenameLen= strlen(filename);
                     newfilenameLen = strlen(newfilename);
                     
                     filename[filenameLen] = '\n';
                     newfilename[newfilenameLen] = '\n';
                     sprintf(wbuf,"RNFR %s",filename);
                     write(sockfd,wbuf,filenameLen+6);

                     

                     nwrite = 0;
                     
                     
                     continue;
                 }
                 else{
                  
                  
                 printf("Invalid input\n");
                 
                 tag=0;
                     sprintf(wbuf,"%s","PASV\n");                   
                     write(sockfd,wbuf,5);
                 
                  continue;
                 }




              } 
                    
         }
         if(FD_ISSET(sockfd,&rset))
         {
         
             bzero(rbuf,strlen(rbuf));
         

             if((nread = recv(sockfd,rbuf,MAXBUF,0)) <0)
                  printf("recv error\n");
             else if(nread == 0)
               break;

             

             if(strncmp(rbuf,"220",3) ==0 || strncmp(rbuf,"530",3)==0)
             {
                

                 


                 
                 
                 strcat(rbuf,"your name:");
                
                 nread += 12;
                 replycode = USERNAME;
             }
             if(strncmp(rbuf,"331",3) == 0)
             {
                ;
                
                strcat(rbuf,"your password:\n");
                nread += 16;
                
                replycode = PASSWORD;
             }
             if(strncmp(rbuf,"230",3) == 0)
             {
                
                replycode = LOGIN;
             }
             if(strncmp(rbuf,"257",3) == 0)
             {
                
                replycode = PATHNAME;  
             }
             if(strncmp(rbuf,"226",3) == 0)
             {
                
                replycode = CLOSEDATA;
             }
             if(strncmp(rbuf,"250",3) == 0)
             {
                
                replycode = ACTIONOK;
             }
             if(strncmp(rbuf,"550",3) == 0)
             {
                replycode = 550;
             }

             
             if(strncmp(rbuf,"350",3) == 0)
             {
                bzero(wbuf,strlen(wbuf));
                sprintf(wbuf,"RNTO %s",newfilename);
                write(sockfd,wbuf,newfilenameLen+6);
                replycode = 350;
             }
             
                 
             
             if(strncmp(rbuf,"227",3) == 0)
             {
                
                

                
                int port1 = strtosrv(rbuf);
                printf("%d\n",port1);
                printf("%s\n",host);

                
                
                data_sock = cliopen(host,port1);
        


                if(tag == 2)
                {
                   write(sockfd,"LIST\n",strlen("LIST\n"));
                   ftp_list(data_sock);
                   
                   
                }
                
                else if(tag == 1)
                {
                    
                    
                    
                    
                    sprintf(wbuf,"RETR %s\n",filename);

                    
                    

                    
                    write(sockfd,wbuf,strlen(wbuf));

                    
                    bzero(rbuf,strlen(rbuf));
                      

                    if((nread = recv(sockfd,rbuf,MAXBUF,0)) <0)
                        printf("recv error\n");

                    
                    if(strncmp(rbuf,"550",3) == 0)
                    {
                      replycode = 550;
                    }
                    
                    else{
                      ftp_get_limit(data_sock,filename);
                    }
                    
                    

                    
                    
                }
                else if(tag == 3)
                {
                    
                    sprintf(wbuf,"STOR %s\n",filename);
                    
                    int handle = open(filename,O_RDWR);
                    

                    
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
                       
                     
                    
                    
                }

                nwrite = 0;
             }
             
             
             if(write(STDOUT_FILENO,rbuf,nread) != nread)
                 printf("write error to stdout\n");
              

              
              if (replycode == PASSWORD)
              {
                set_disp_mode(STDIN_FILENO,0);  
              }
              else{
                set_disp_mode(STDIN_FILENO,1);  
              }

                         
         }
    }
}
