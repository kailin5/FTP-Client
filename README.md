# FTP-Client
This is the repo of FTP client of Internet Applications

# Reset password to *
https://zhidao.baidu.com/question/166454029.html

# Bug to be fixed:
The user name will be sent to ftp server twice, second time is not include command USER uftp

# Finished work to date
1: Scucessfully connected, wireshark can have the packet
2: Overall framework has finished, minor bugs to be fixed

# How to use?
	2 XShell opened.
1: ftpcli
2: wireshark

ftpcli: gcc ftpcli.c 
./a.out 10.3.255.85   (可以改文件名)

wireshark 抓包