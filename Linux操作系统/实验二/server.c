#define _POSIX_C_SOURCE 200809L

/*服务器端程序：*/
/* 1. 通过命名管道接收客户端请求*/
/* 2. 通过 SIGUSR1 获知有新请求到达*/
/* 3. 在信号量保护下更新共享内存*/
/* 4. 通过消息队列向指定客户端返回处理结果*/

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
#include <sys/stat.h>
#include <unistd.h>

//semctl() 设置初值时使用的联合体
union semun {
    int val;
    struct semid_ds *buf;
    unsigned short *array;
};

//对第 0 个信号量执行操作：operation 为 -1 时执行 P 操作，为 1 时执行 V 操作
static int semaphore_change(int semid, short operation)
{
    //SEM_UNDO 使进程异常退出时，内核能够撤销未恢复的信号量操作
    struct sembuf op = {0, operation, SEM_UNDO};

    //semop 被其他信号中断时重试，其他错误则报告并返回
    while (semop(semid, &op, 1) == -1) {
        if (errno != EINTR) {
            perror("semop");
            return -1;
        }
    }
    return 0;
}

//处理一条完整的管道请求，并通过消息队列回复客户端
static int process_request(int semid, int msgid, shared_record *shared,
                           const pipe_request *request)
{
    queue_reply reply = {0};

    //PID 必须为正数，否则无法作为有效的回复消息类型
    if (request->client_pid <= 0)
        return 0;

    //P 操作：获得共享内存的独占访问权
    if (semaphore_change(semid, -1) == -1)
        return -1;

    //保存最新请求，并累计服务器已经处理的请求数量
    shared->writer_pid = getpid();
    shared->client_pid = request->client_pid;
    shared->request_id = request->request_id;
    shared->total_requests++;
    snprintf(shared->text, sizeof(shared->text), "%s", request->text);

    //V 操作：共享内存更新完成，释放访问权
    if (semaphore_change(semid, 1) == -1)
        return -1;

    printf("收到客户端 %ld 的第 %u 条消息：%s\n",
           (long)request->client_pid, request->request_id, request->text);
    fflush(stdout);

    //使用客户端 PID 作为消息类型
    //每个客户端只接收与自身 PID 相同类型的回复
    reply.mtype = (long)request->client_pid;
    reply.request_id = request->request_id;
    reply.status = 0;
    snprintf(reply.text, sizeof(reply.text),
             "服务器已处理第 %u 条消息", request->request_id);

    //发送回复；如果调用被信号中断，则继续尝试
    while (msgsnd(msgid, &reply, QUEUE_REPLY_SIZE, 0) == -1) {
        if (errno != EINTR) {
            perror("msgsnd");
            return -1;
        }
    }
    return 0;
}

//一次唤醒后持续读取 FIFO，直到当前已经没有可读请求
static int drain_fifo(int fifo_fd, int semid, int msgid,
                      shared_record *shared)
{
    for (;;) {
        pipe_request request;
        // 请求结构体小于 PIPE_BUF，客户端一次写入时不会与其他请求交叉
        ssize_t count = read(fifo_fd, &request, sizeof(request));

        if (count == -1) {
            if (errno == EINTR)
                continue;
            //非阻塞 FIFO 暂时没有数据时，本轮读取结束
            if (errno == EAGAIN || errno == EWOULDBLOCK)
                return 0;
            perror("read fifo");
            return -1;
        }
        if (count == 0)
            return 0;
        //只处理长度与 pipe_request 完全相同的完整请求
        if ((size_t)count != sizeof(request)) {
            fprintf(stderr, "收到不完整的管道请求，已忽略。\n");
            continue;
        }

        //强制保证文本以空字符结束，防止越界读取
        request.text[TEXT_SIZE - 1] = '\0';
        if (process_request(semid, msgid, shared, &request) == -1)
            return -1;
    }
}

int main(void)
{
    //三种 System V IPC 资源使用各自的键值和标识符
    key_t shm_key, sem_key, msg_key;
    int fifo_fd = -1;
    int shmid = -1;
    int semid = -1;
    int msgid = -1;
    int exit_status = EXIT_FAILURE;
    shared_record *shared = (void *)-1;
    FILE *pid_file = NULL;
    sigset_t wait_set;
    union semun sem_value;

    //先阻塞这些信号，再发布 PID，避免客户端通知信号提前到达
    sigemptyset(&wait_set);
    sigaddset(&wait_set, SIGUSR1);
    sigaddset(&wait_set, SIGINT);
    sigaddset(&wait_set, SIGTERM);
    if (sigprocmask(SIG_BLOCK, &wait_set, NULL) == -1) {
        perror("sigprocmask");
        goto cleanup;
    }

    //创建仅当前用户可读写的命名管道
    if (mkfifo(FIFO_PATH, 0600) == -1 && errno != EEXIST) {
        perror("mkfifo");
        goto cleanup;
    }

    //服务器以读写和非阻塞方式打开 FIFO：O_RDWR 避免没有客户端时产生文件结束，O_NONBLOCK 避免读取阻塞。
    fifo_fd = open(FIFO_PATH, O_RDWR | O_NONBLOCK);
    if (fifo_fd == -1) {
        perror("open fifo");
        goto cleanup;
    }

    //使用同一路径和不同项目编号生成互不相同的 IPC 键值
    shm_key = ftok(FIFO_PATH, SHM_PROJ_ID);
    sem_key = ftok(FIFO_PATH, SEM_PROJ_ID);
    msg_key = ftok(FIFO_PATH, MSG_PROJ_ID);
    if (shm_key == -1 || sem_key == -1 || msg_key == -1) {
        perror("ftok");
        goto cleanup;
    }

    //IPC_EXCL 防止同时启动两个服务器并操作同一共享内存
    shmid = shmget(shm_key, sizeof(shared_record),
                   IPC_CREAT | IPC_EXCL | 0600);
    if (shmid == -1) {
        perror("shmget");
        goto cleanup;
    }

    //将共享内存连接到服务器地址空间，并清空初始内容
    shared = shmat(shmid, NULL, 0);
    if (shared == (void *)-1) {
        perror("shmat");
        goto cleanup;
    }
    memset(shared, 0, sizeof(*shared));

    //创建只包含一个信号量的信号量集
    semid = semget(sem_key, 1, IPC_CREAT | IPC_EXCL | 0600);
    if (semid == -1) {
        perror("semget");
        goto cleanup;
    }
    //初始值为 1，表示共享内存当前可以被一个进程访问
    sem_value.val = 1;
    if (semctl(semid, 0, SETVAL, sem_value) == -1) {
        perror("semctl SETVAL");
        goto cleanup;
    }

    //创建用于服务器向客户端返回结果的消息队列
    msgid = msgget(msg_key, IPC_CREAT | IPC_EXCL | 0600);
    if (msgid == -1) {
        perror("msgget");
        goto cleanup;
    }

    //保存服务器 PID，客户端读取后使用 kill() 发送通知信号
    pid_file = fopen(PID_PATH, "w");
    if (pid_file == NULL) {
        perror("fopen pid file");
        goto cleanup;
    }
    fprintf(pid_file, "%ld\n", (long)getpid());
    if (fclose(pid_file) == EOF) {
        pid_file = NULL;
        perror("fclose pid file");
        goto cleanup;
    }
    pid_file = NULL;

    printf("服务器已启动，PID=%ld。按 Ctrl+C 退出。\n", (long)getpid());
    fflush(stdout);
    exit_status = EXIT_SUCCESS;

    //主循环同步等待客户端通知信号或服务器退出信号
    for (;;) {
        int signal_number;

        if (sigwait(&wait_set, &signal_number) != 0) {
            fprintf(stderr, "sigwait 失败。\n");
            exit_status = EXIT_FAILURE;
            break;
        }
        // Ctrl+C 产生 SIGINT；SIGTERM 也要求服务器安全退出
        if (signal_number == SIGINT || signal_number == SIGTERM)
            break;
        // SIGUSR1 表示客户端已经向 FIFO 写入新请求
        if (signal_number == SIGUSR1 &&
            drain_fifo(fifo_fd, semid, msgid, shared) == -1) {
            exit_status = EXIT_FAILURE;
            break;
        }
    }

cleanup:
    //无论服务器正常退出还是初始化失败，都尝试释放已经创建的资源
    //删除顺序为 PID 文件、共享内存连接、三种 System V IPC 和 FIFO
    if (pid_file != NULL)
        fclose(pid_file);
    unlink(PID_PATH);
    if (shared != (void *)-1)
        shmdt(shared);
    if (msgid != -1)
        msgctl(msgid, IPC_RMID, NULL);
    if (semid != -1)
        semctl(semid, 0, IPC_RMID);
    if (shmid != -1)
        shmctl(shmid, IPC_RMID, NULL);
    if (fifo_fd != -1)
        close(fifo_fd);
    unlink(FIFO_PATH);

    puts("服务器已退出，IPC 资源已清理。");
    return exit_status;
}
