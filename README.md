# FTP-Client
This is the repo of FTP client of Internet Applications

# functions to be added:
delete, rename, mkdir,ascii, binary
## extended function
limitiing download rate
https://blog.csdn.net/wk_bjut_edu_cn/article/details/86506846

# Finished work to date
1: Scucessfully connected, wireshark can have the packet
2: Overall framework has finished, minor bugs to be fixed
3: username, password, ls, cd, pwd, put, get, put function has been implemented

# How to use?
	2 XShell opened.
1: ftpcli
2: wireshark

ftpcli: gcc ftpcli.c 
./a.out 10.3.255.85   (可以改文件名)

wireshark 抓包