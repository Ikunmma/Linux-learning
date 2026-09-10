#include <signal.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include "signal_handler.h"

// 全局退出标志：0 表示继续运行，1 表示请求退出
volatile sig_atomic_t exit_requested = 0;

// SIGINT 信号处理函数，用户按 Ctrl+C 时执行
static void handle_sigint(int sig) {
    (void)sig;  // 本程序不需要使用信号编号，避免编译器产生警告

    // 设置退出标志，通知主程序结束运行
    exit_requested = 1;

    const char msg[] =
        "\n[Signal] 收到 SIGINT，准备安全退出...\n";

    // 在信号处理函数中输出提示信息
    write(STDOUT_FILENO, msg, sizeof(msg) - 1);
}

// 设置 SIGINT 信号的处理方式
void setup_signal_handler(void) {
    struct sigaction sa;

    // 将 sigaction 结构体初始化为 0
    memset(&sa, 0, sizeof(sa));

    // 指定收到 SIGINT 后调用 handle_sigint()
    sa.sa_handler = handle_sigint;

    // 初始化信号屏蔽集合
    sigemptyset(&sa.sa_mask);

    // 不设置 SA_RESTART，使阻塞的输入操作能够被 SIGINT 打断
    sa.sa_flags = 0;

    // 注册 SIGINT 信号处理函数
    if (sigaction(SIGINT, &sa, NULL) == -1) {
        perror("sigaction");
    }
}