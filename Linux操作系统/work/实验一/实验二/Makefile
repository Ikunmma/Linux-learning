#指定使用GCC编译器
CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -Wpedantic -O2
#声明命令目标
.PHONY: all clean
#默认生成服务器和客户端
all: server client
#编译服务器程序
server: server.c ipc_common.h
	$(CC) $(CFLAGS) server.c -o server
#编译客户端程序
client: client.c ipc_common.h
	$(CC) $(CFLAGS) client.c -o client
#删除生成的可执行文件
clean:
	rm -f server client