#ifndef IPC_COMMON_H
#define IPC_COMMON_H

#include <sys/types.h>

//名管道用于客户端向服务器发送请求
#define FIFO_PATH "/tmp/ipc_all_demo.fifo"
//PID 文件保存服务器进程号，客户端根据它发送 SIGUSR1 信号
#define PID_PATH  "/tmp/ipc_all_demo.pid"
//所有消息文本缓冲区的统一大小
#define TEXT_SIZE 256

//使用同一个 FIFO 路径和不同项目编号生成三种 System V IPC 键值
#define SHM_PROJ_ID 'S'
#define SEM_PROJ_ID 'E'
#define MSG_PROJ_ID 'M'

//客户端通过命名管道发送给服务器的请求
typedef struct {
    pid_t client_pid;            //发送请求的客户端进程号
    unsigned int request_id;     //当前客户端内部递增的请求编号
    char text[TEXT_SIZE];        //客户端输入的文本
} pipe_request;

//服务器在共享内存中保存的最新通信记录。
typedef struct {
    pid_t writer_pid;            //写入共享内存的服务器进程号
    pid_t client_pid;            //最近一次请求的客户端进程号
    unsigned int request_id;     //最近一次请求的编号
    unsigned long total_requests;//服务器累计处理的请求数量
    char text[TEXT_SIZE];        //最近一次请求的文本
} shared_record;

//服务器通过消息队列返回给客户端的处理结果
typedef struct {
    long mtype;                  //消息类型，设置为目标客户端 PID
    unsigned int request_id;     //对应的客户端请求编号
    int status;                  //处理状态，0 表示处理成功
    char text[TEXT_SIZE];        //服务器回复文本
} queue_reply;

// msgsnd 和 msgrcv 的长度参数不包含结构体开头的 long mtype
#define QUEUE_REPLY_SIZE (sizeof(queue_reply) - sizeof(long))

#endif
