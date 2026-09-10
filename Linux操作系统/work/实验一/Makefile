# 指定使用 GCC 编译器
CC = gcc

# 设置编译选项，开启常用警告信息
CFLAGS = -Wall -Wextra

# 将三个目标文件链接生成最终可执行文件 main
main: main.o task.o signal_handler.o
	$(CC) -o main main.o task.o signal_handler.o

# 编译 main.c，生成 main.o
main.o: main.c
	$(CC) $(CFLAGS) -c main.c

# 编译 task.c，生成 task.o
task.o: task.c
	$(CC) $(CFLAGS) -c task.c

# 编译 signal_handler.c，生成 signal_handler.o
signal_handler.o: signal_handler.c
	$(CC) $(CFLAGS) -c signal_handler.c

# 删除编译过程中产生的目标文件和可执行文件
clean:
	rm -f *.o main