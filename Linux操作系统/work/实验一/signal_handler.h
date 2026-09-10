#ifndef SIGNAL_HANDLER_H
#define SIGNAL_HANDLER_H

#include <signal.h>

// 声明全局退出标志，供主程序和信号处理程序共同使用
extern volatile sig_atomic_t exit_requested;

// 声明 SIGINT 信号处理初始化函数
void setup_signal_handler(void);

#endif