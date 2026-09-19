#define _POSIX_C_SOURCE 200809L

/* 客户端程序：*/
/* 1. 通过命名管道向服务器发送请求*/
/* 2. 通过 SIGUSR1 通知服务器读取请求*/
/* 3. 通过消息队列接收服务器回复*/
/* 4. 在信号量保护下读取共享内存中的最新记录*/

#include "ipc_common.h"

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <unistd.h>

//对第 0 个信号量执行操作：operation 为 -1 时执行 P 操作，为 1 时执行 V 操作
static int semaphore_change(int semid, short operation)
{
    //SEM_UNDO 可在客户端异常退出时撤销未恢复的信号量操作
    struct sembuf op = {0, operation, SEM_UNDO};

    //被信号中断时重新执行，其他错误直接返回
    while (semop(semid, &op, 1) == -1) {
        if (errno != EINTR) {
            perror("semop");
            return -1;
        }
    }
    return 0;
}

//从 PID 文件中读取服务器进程号//
static int read_server_pid(pid_t *server_pid)
{
    FILE *file = fopen(PID_PATH, "r");
    long value;

    if (file == NULL) {
        perror("fopen pid file");
        return -1;
    }
    //PID 文件必须包含一个大于 0 的整数
    if (fscanf(file, "%ld", &value) != 1 || value <= 0) {
        fprintf(stderr, "服务器 PID 文件无效。\n");
        fclose(file);
        return -1;
    }
    fclose(file);
    *server_pid = (pid_t)value;
    return 0;
}

int main(void)
{
    //三种 System V IPC 资源的键值和标识符
    key_t shm_key, sem_key, msg_key;
    int fifo_fd = -1;
    int shmid = -1;
    int semid = -1;
    int msgid = -1;
    int exit_status = EXIT_FAILURE;
    pid_t server_pid;
    shared_record *shared = (void *)-1;
    unsigned int request_id = 0;
    char input[TEXT_SIZE];

    //先读取服务器 PID，再用 kill(pid, 0) 检查该进程是否存在
    if (read_server_pid(&server_pid) == -1)
        goto cleanup;
    if (kill(server_pid, 0) == -1) {
        perror("服务器未运行");
        goto cleanup;
    }

    //使用与服务器相同的路径和项目编号生成 IPC 键值
    shm_key = ftok(FIFO_PATH, SHM_PROJ_ID);
    sem_key = ftok(FIFO_PATH, SEM_PROJ_ID);
    msg_key = ftok(FIFO_PATH, MSG_PROJ_ID);
    if (shm_key == -1 || sem_key == -1 || msg_key == -1) {
        perror("ftok");
        goto cleanup;
    }

    //连接服务器已经创建的共享内存、信号量和消息队列
    shmid = shmget(shm_key, sizeof(shared_record), 0600);
    semid = semget(sem_key, 1, 0600);
    msgid = msgget(msg_key, 0600);
    if (shmid == -1 || semid == -1 || msgid == -1) {
        perror("连接 IPC 资源");
        goto cleanup;
    }

    //客户端只读取共享内存，因此使用 SHM_RDONLY
    shared = shmat(shmid, NULL, SHM_RDONLY);
    if (shared == (void *)-1) {
        perror("shmat");
        goto cleanup;
    }

    //命名管道只用于客户端向服务器发送请求
    fifo_fd = open(FIFO_PATH, O_WRONLY);
    if (fifo_fd == -1) {
        perror("open fifo");
        goto cleanup;
    }

    puts("输入消息并回车；输入 quit 退出。");
    exit_status = EXIT_SUCCESS;

    //循环读取用户输入，直到输入 quit 或输入流结束
    for (;;) {
        pipe_request request = {0};
        queue_reply reply = {0};
        size_t length;
        ssize_t count;

        printf("client> ");
        fflush(stdout);
        //fgets 会为输入文本保留结尾空字符，避免缓冲区溢出
        if (fgets(input, sizeof(input), stdin) == NULL)
            break;

        //删除正常输入末尾的换行符
        length = strlen(input);
        if (length > 0 && input[length - 1] == '\n') {
            input[--length] = '\0';
        } else if (!feof(stdin)) {
            //缓冲区中没有换行符说明输入过长。丢弃本行剩余字符，防止它们被当成下一条请求
            int ch;
            while ((ch = getchar()) != '\n' && ch != EOF) {}
            fprintf(stderr, "消息过长，最多输入 %d 字节。\n", TEXT_SIZE - 2);
            continue;
        }

        //quit 只用于退出客户端，不会发送给服务器
        if (strcmp(input, "quit") == 0)
            break;
        //空行不生成请求
        if (length == 0)
            continue;

        //构造请求；每个客户端分别维护自己的递增请求编号
        request.client_pid = getpid();
        request.request_id = ++request_id;
        memcpy(request.text, input, length + 1);

        //请求结构体小于 PIPE_BUF，一次 write 可保证多客户端写入时
        //每个完整请求不会与其他客户端的请求内容交叉
        do {
            count = write(fifo_fd, &request, sizeof(request));
        } while (count == -1 && errno == EINTR);
        if (count != (ssize_t)sizeof(request)) {
            perror("write fifo");
            exit_status = EXIT_FAILURE;
            break;
        }

        //管道写入完成后，发送 SIGUSR1 通知服务器读取请求
        if (kill(server_pid, SIGUSR1) == -1) {
            perror("kill SIGUSR1");
            exit_status = EXIT_FAILURE;
            break;
        }

        //只接收消息类型等于自身 PID 的回复，防止多个客户端互相取走对方的消息
        while (msgrcv(msgid, &reply, QUEUE_REPLY_SIZE,
                      (long)getpid(), 0) == -1) {
            if (errno != EINTR) {
                perror("msgrcv");
                exit_status = EXIT_FAILURE;
                goto cleanup;
            }
        }
        reply.text[TEXT_SIZE - 1] = '\0';
        printf("消息队列回复：%s\n", reply.text);

        //P 操作：读取共享内存前获得访问权
        if (semaphore_change(semid, -1) == -1) {
            exit_status = EXIT_FAILURE;
            break;
        }
        printf("共享内存内容：客户端=%ld，请求=%u，总请求数=%lu，文本=%s\n",
               (long)shared->client_pid, shared->request_id,
               shared->total_requests, shared->text);
        //V 操作：读取完成后释放访问权
        if (semaphore_change(semid, 1) == -1) {
            exit_status = EXIT_FAILURE;
            break;
        }
    }

cleanup:
    //客户端只关闭自身连接，不删除由服务器管理的 IPC 资源
    if (fifo_fd != -1)
        close(fifo_fd);
    if (shared != (void *)-1)
        shmdt(shared);
    return exit_status;
}
