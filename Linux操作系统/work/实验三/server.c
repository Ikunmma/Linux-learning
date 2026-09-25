#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <dirent.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <signal.h>
#define PORT 8888
#define BUFFER_SIZE 1024
#define SERVER_DIR "server_files"
//创建服务器保存文件的目录
void create_server_dir(){
    if (mkdir(SERVER_DIR, 0755) == -1 && errno != EEXIST){
        perror("mkdir");
        exit(1);
    }
}
//处理客户端发送的文本消息
void handle_message(int client_fd){
    char buffer[BUFFER_SIZE];
    int n = recv(client_fd,buffer,sizeof(buffer) - 1,0);
    if (n <= 0)
        return;
    buffer[n] = '\0';
    printf("收到客户端消息：%s\n", buffer);
    send(client_fd,
         "服务器已收到消息",
         strlen("服务器已收到消息") + 1,
         0);
}
//处理客户端上传文件
void handle_upload(int client_fd){
    char filename[256];
    char filepath[512];
    char buffer[BUFFER_SIZE];
    int file_size;
    int received = 0;
    //接收文件名
    recv(client_fd,filename,sizeof(filename),0);
    //接收文件大小
    recv(client_fd,&file_size,sizeof(file_size),0);
    //拼接服务器保存路径
    snprintf(filepath,
             sizeof(filepath),
             "%s/%s",
             SERVER_DIR,
             filename);

    //创建文件
    FILE *fp = fopen(filepath, "wb");
    if (fp == NULL){
        perror("fopen");
        return;
    }
    //循环接收文件内容
    while (received < file_size){
        int need = BUFFER_SIZE;
        if (file_size - received < BUFFER_SIZE){
            need = file_size - received;
        }
        int n = recv(client_fd,buffer,need,0);
        if (n <= 0)
            break;
        //将收到的数据保存到文件
        fwrite(buffer,1,n,fp);
        received += n;
    }
    fclose(fp);
    printf("文件上传成功：%s\n",filename);
    send(client_fd,"文件上传成功",strlen("文件上传成功") + 1,0);
}
//处理客户端下载文件
void handle_download(int client_fd){
    char filename[256];
    char filepath[512];
    char buffer[BUFFER_SIZE];
    //接收客户端下载的文件名
    recv(client_fd,filename,sizeof(filename),0);
    snprintf(filepath,sizeof(filepath),"%s/%s",SERVER_DIR,filename);
    //打开服务器文件
    FILE *fp = fopen(filepath, "rb");
    //文件不存在
    if (fp == NULL){
        int status = 0;
        send(client_fd,&status,sizeof(status),0);
        return;
    }
    //告诉客户端文件存在
    int status = 1;
    send(client_fd,&status,sizeof(status),0);
    //获取文件大小
    fseek(fp,0,SEEK_END);
    int file_size = (int)ftell(fp);
    rewind(fp);
    //发送文件大小
    send(client_fd,&file_size,sizeof(file_size),0);
    //循环读取文件并发送
    while (1){
        int n = fread(buffer,1,sizeof(buffer),fp);
        if (n <= 0)
            break;
        send(client_fd,buffer,n,0);
    }
    fclose(fp);
    printf("客户端下载文件：%s\n",
           filename);
}
//将服务器文件列表发送给客户端
void handle_list(int client_fd){
    DIR *dir;
    struct dirent *entry;
    char list[4096] = "";
    dir = opendir(SERVER_DIR);
    if (dir == NULL){
        strcpy(list,"无法打开服务器目录\n");
        send(client_fd,list,strlen(list) + 1,0);
        return;
    }
    //遍历目录中的文件
    while ((entry = readdir(dir)) != NULL){
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0){
            continue;
        }
        strcat(list,entry->d_name);
        strcat(list,"\n");
    }
    closedir(dir);
    if (strlen(list) == 0){
        strcpy(list,"服务器暂无文件\n");
    }
    send(client_fd,list,strlen(list) + 1,0);
}
//处理一个客户端的所有操作
void handle_client(int client_fd){
    int command;
    while (1){
        //接收客户端操作编号
        int n = recv(client_fd,&command,sizeof(command),0);
        if (n <= 0)
            break;
        switch (command){
            case 1:
                handle_message(client_fd);
                break;
            case 2:
                handle_upload(client_fd);
                break;
            case 3:
                handle_download(client_fd);
                break;
            case 4:
                handle_list(client_fd);
                break;
            case 0:
                printf("客户端退出\n");
                close(client_fd);
                return;
            default:
                break;
        }
    }
    close(client_fd);
}
int main(){
    int server_fd;
    int client_fd;
    signal(SIGCHLD, SIG_IGN);
    struct sockaddr_in server_addr;
    struct sockaddr_in client_addr;
    socklen_t client_len;
    //创建服务器文件目录
    create_server_dir();
    //创建 TCP 套接字
    server_fd = socket(AF_INET,SOCK_STREAM,0);
    if (server_fd == -1){
        perror("socket");
        return 1;
    }
    //初始化服务器地址结构
    memset(&server_addr,0,sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(PORT);
    //将套接字与端口绑定
    if (bind(server_fd,(struct sockaddr *)&server_addr,sizeof(server_addr)) == -1){
        perror("bind");
        close(server_fd);
        return 1;
    }
    //开始监听客户端连接
    if (listen(server_fd, 5) == -1){
        perror("listen");
        close(server_fd);
        return 1;
    }
    printf("============================\n");
    printf("      TCP 文件服务器\n");
    printf("============================\n");
    printf("服务器已启动\n");
    printf("监听端口：%d\n", PORT);
    printf("等待客户端连接...\n\n");
    while (1){
        client_len = sizeof(client_addr);
        //等待客户端连接
        client_fd = accept(server_fd,(struct sockaddr *)&client_addr,&client_len);
        if (client_fd == -1){
            perror("accept");
            continue;
        }
        printf("客户端连接成功：%s:%d\n",inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
        //创建子进程处理客户端
        pid_t pid = fork();
        if (pid == -1){
            perror("fork");
            close(client_fd);
            continue;
        }
        if (pid == 0){
            //子进程不需要监听套接字
            close(server_fd);
            handle_client(client_fd);
            exit(0);
        }
        //父进程不处理当前客户端，继续等待新的客户端连接
        close(client_fd);
    }
    return 0;
}