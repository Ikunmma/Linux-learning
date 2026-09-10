#include <stdio.h>
#include "task.h"
#include "signal_handler.h"

int main(void) {
    int choice;  // 保存用户选择的任务编号

    // 注册 SIGINT 信号处理函数，用于处理 Ctrl+C
    setup_signal_handler();

    // 未收到退出请求时，循环显示任务菜单
    while (!exit_requested) {
        printf("\n========== Linux Task Manager ==========\n");
        printf("1. 显示当前日期和时间\n");
        printf("2. 显示 Linux 系统信息\n");
        printf("3. 显示当前登录用户\n");
        printf("4. 显示当前工作目录\n");
        printf("0. 退出程序\n");
        printf("========================================\n");
        printf("请选择任务: ");
        fflush(stdout);  // 立即将提示信息输出到终端

        // 读取用户输入，并判断是否成功读取一个整数
        if (scanf("%d", &choice) != 1) {

            // 如果输入操作是被 Ctrl+C 中断，则退出循环
            if (exit_requested) {
                break;
            }

            // 清除输入流错误状态
            clearerr(stdin);

            // 清除本次输入中剩余的非法字符
            int ch;
            while ((ch = getchar()) != '\n' && ch != EOF);

            printf("输入错误，请输入 0~4 之间的数字。\n");
            continue;
        }

        // 用户输入 0，正常退出程序
        if (choice == 0) {
            printf("任务管理器正常退出。\n");
            break;
        }

        // 判断任务编号是否在有效范围内
        if (choice < 1 || choice > 4) {
            printf("无效任务，请重新选择。\n");
            continue;
        }

        // 根据用户选择创建子进程并执行对应任务
        run_task(choice);
    }

    // 如果因为收到 SIGINT 信号退出，则输出提示信息
    if (exit_requested) {
        printf("任务管理器已安全退出。\n");
    }

    return 0;  // 主程序正常结束
}