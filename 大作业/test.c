/***************************************************************
File Name: test_pwd.c
Author: xiaohuo
Function List: main() 主函数
Created Time: 2018年7月3日
**************************************************************/
 
#include <stdio.h>
#include <termios.h>
#include <unistd.h>
 
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
 
int main()
{
    char passWord[20];
    printf("请输入密码:\n");
    getpasswd(passWord, 20);
    printf("\n");
    printf("你输入的密码是:%s\n", passWord);
    return 0;
}
