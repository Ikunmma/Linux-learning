#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include "task.h"

// 根据用户选择创建子进程，并执行对应的 Linux 系统程序
void run_task(int choice) {
    pid_t pid;   // 保存 fork() 的返回值
    int status;  // 保存子进程的结束状态

    printf("[Manager] 正在创建任务进程...\n");

    // 创建一个新的子进程
    pid = fork();

    // fork() 返回值小于 0，表示创建子进程失败
    if (pid < 0) {
        perror("fork");
        return;
    }

    // fork() 返回 0，表示当前代码运行在子进程中
    if (pid == 0) {
        printf("[Worker] 子进程 PID = %d\n", getpid());
        fflush(stdout);

        // 根据用户选择执行不同的 Linux 系统程序
        switch (choice) {
            case 1:
                // 用 date 程序替换当前子进程，显示日期和时间
                execl("/bin/date", "date", NULL);
                break;

            case 2:
                // 执行 uname -a，显示 Linux 系统信息
                execl("/usr/bin/uname", "uname", "-a", NULL);
                break;

            case 3:
                // 执行 whoami，显示当前登录用户
                execl("/usr/bin/whoami", "whoami", NULL);
                break;

            case 4:
                // 执行 pwd，显示当前工作目录
                execl("/bin/pwd", "pwd", NULL);
                break;

            default:
                exit(EXIT_FAILURE);
        }

        // exec 执行成功后不会返回；
        // 如果执行到这里，说明 exec 执行失败
        perror("exec");
        exit(EXIT_FAILURE);
    }
    else {
        // 父进程等待子进程执行结束，并回收子进程
        waitpid(pid, &status, 0);

        // 判断子进程是否正常结束
        if (WIFEXITED(status)) {
            printf("[Manager] 任务进程 %d 执行结束，返回值 = %d\n",
                   pid, WEXITSTATUS(status));
        }
        else {
            printf("[Manager] 任务进程 %d 异常结束\n", pid);
        }
    }
}