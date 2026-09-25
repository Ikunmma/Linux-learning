#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/stat.h>
#define PORT 8888
#define BUFFER_SIZE 1024
#define DOWNLOAD_DIR "downloads"
//创建客户端下载文件目录
void create_download_dir(){
    if (mkdir(DOWNLOAD_DIR, 0755) == -1 && errno != EEXIST){
        perror("mkdir");
    }
}
//发送文本消息
void send_message(int sockfd){
    char message[BUFFER_SIZE];
    char reply[BUFFER_SIZE];
    printf("请输入消息：");
    getchar();
    fgets(message,sizeof(message),stdin);
    message[strcspn(message, "\n")] = '\0';
    int command = 1;
    //先告诉服务器当前操作是发送消息
    send(sockfd,&command,sizeof(command),0);
    //发送真正的消息
    send(sockfd,message,strlen(message) + 1,0);
    //接收服务器回复
    recv(sockfd,reply,sizeof(reply),0);
    printf("服务器回复：%s\n",reply);
}
//上传文件
void upload_file(int sockfd){
    char path[256];
    char filename[256];
    char buffer[BUFFER_SIZE];
    printf("请输入文件路径：");
    scanf("%255s",path);
    //打开本地文件
    FILE *fp = fopen(path, "rb");
    if (fp == NULL){
        printf("文件不存在\n");
        return;
    }
    //从路径中提取文件名
    char *p = strrchr(path, '/');
    if (p != NULL){
        strcpy(filename,
               p + 1);
    }
    else{
        strcpy(filename,
               path);
    }
    //获取文件大小
    fseek(fp,0,SEEK_END);
    int file_size =(int)ftell(fp);
    rewind(fp);
    int command = 2;
    //告诉服务器当前操作是上传文件
    send(sockfd,&command,sizeof(command),0);
    //发送文件名
    send(sockfd,filename,sizeof(filename),0);
    //发送文件大小
    send(sockfd,&file_size,sizeof(file_size),0);
    //循环读取文件并发送
    while (1){
        int n = fread(buffer,1,sizeof(buffer),fp);
        if (n <= 0)
            break;
        send(sockfd,buffer,n,0);
    }
    fclose(fp);
    //等待服务器确认
    char reply[BUFFER_SIZE];
    recv(sockfd,reply,sizeof(reply),0);
    printf("服务器回复：%s\n",reply);
}
//下载文件
void download_file(int sockfd){
    char filename[256];
    char filepath[512];
    char buffer[BUFFER_SIZE];
    int status;
    int file_size;
    int received = 0;
    printf("请输入服务器文件名：");
    scanf("%255s",filename);
    int command = 3;
    //告诉服务器当前操作是下载
    send(sockfd,&command,sizeof(command),0);
    //发送要下载的文件名
    send(sockfd,filename,sizeof(filename),0);
    //接收服务器返回的文件状态
    recv(sockfd,&status,sizeof(status),0);
    if (status == 0){
        printf("服务器中不存在该文件\n");
        return;
    }
    //接收文件大小
    recv(sockfd,&file_size,sizeof(file_size),0);
    //创建本地 downloads 目录
    create_download_dir();
    //拼接文件保存路径
    snprintf(filepath,sizeof(filepath),"%s/%s",DOWNLOAD_DIR,filename);
    //创建本地文件
    FILE *fp = fopen(filepath, "wb");
    if (fp == NULL){
        printf("无法创建下载文件\n");
        return;
    }
    //循环接收文件
    while (received < file_size){
        int need = BUFFER_SIZE;
        if (file_size - received < BUFFER_SIZE){
            need = file_size - received;
        }
        int n = recv(sockfd,buffer,need,0);
        if (n <= 0)
            break;
        //保存收到的文件数据
        fwrite(buffer,1,n,fp);
        received += n;
    }
    fclose(fp);
    printf("文件下载成功\n");
    printf("保存位置：%s\n",filepath);
}
//查看服务器文件列表
void list_files(int sockfd){
    int command = 4;
    char list[4096];
    //告诉服务器查看文件列表
    send(sockfd,&command,sizeof(command),0);
    //接收服务器文件列表
    recv(sockfd,list,sizeof(list),0);
    printf("\n");
    printf("============================\n");
    printf("       服务器文件列表\n");
    printf("============================\n");
    printf("%s",list);
    printf("============================\n");
}
//显示客户端菜单
void show_menu(){
    printf("\n");
    printf("============================\n");
    printf("      TCP 文件通信客户端\n");
    printf("============================\n");
    printf("1. 发送文本消息\n");
    printf("2. 上传文件\n");
    printf("3. 下载文件\n");
    printf("4. 查看服务器文件列表\n");
    printf("0. 退出\n");
    printf("============================\n");
    printf("请输入操作：");
}
int main(int argc, char *argv[]){
    int sockfd;
    struct sockaddr_in server_addr;
    if (argc != 2){
        printf("使用方法：%s 服务器IP\n",argv[0]);
        printf("例如：%s 127.0.0.1\n",argv[0]);
        return 1;
    }
    //创建 TCP 套接字 
    sockfd = socket(AF_INET,SOCK_STREAM,0);
    if (sockfd == -1){
        perror("socket");
        return 1;
    }
    //初始化服务器地址
    memset(&server_addr,0,sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    //将字符串形式的IP转换成网络地址
    inet_pton(AF_INET,argv[1],&server_addr.sin_addr);
    //连接服务器
    if (connect(sockfd,(struct sockaddr *)&server_addr,sizeof(server_addr)) == -1){
        perror("connect");
        close(sockfd);
        return 1;
    }
    printf("连接服务器成功！\n");
    while (1){
        int choice;
        show_menu();
        scanf("%d",&choice);
        switch (choice){
            case 1:
                send_message(sockfd);
                break;
            case 2:
                upload_file(sockfd);
                break;
            case 3:
                download_file(sockfd);
                break;
            case 4:
                list_files(sockfd);
                break;
            case 0:{
                int command = 0;
                send(sockfd,&command,sizeof(command),0);
                printf("客户端退出\n");
                close(sockfd);
                return 0;
            }
            default:
                printf("输入错误，请重新选择\n");
                break;
        }
    }
}