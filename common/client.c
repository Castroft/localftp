// client.c
#include "discovery.h"
#include "transfer.h"
#include <sys/stat.h>
#include <dirent.h>
#include <fcntl.h>
#include <libgen.h>


// 发送文件给服务器， 返回 0 表示成功
int send_tcp_file(const char* filename, const char* ip, int port, const char*username, const char* password)
{
    int sockfd;
    struct stat file_stat;

    // 检查文件是否存在
    if(stat(filename, &file_stat) != 0)
    {
        printf("File not found: %s \n", filename);
        return -1;
    }

    if(!S_ISREG(file_stat.st_mode))
    {
        printf("Not a regular file: %s \n", filename);
        return -1;
    }

    // 连接到服务器
    sockfd = open_clientfd(ip, port);
    if(sockfd < 0)
    {
        return -1;
    }
    printf("Connect to %s:%d\n", ip, port);

    // 发送认证消息
    if(send_auth_request(sockfd, username, password) < 0) 
    {
        printf("Authentication failed \n");
        close(sockfd);
        return -1;
    }

    // 接受认证响应
    if(receive_auth_reponse(sockfd) <= 0)
    {
        printf("Authentication rejected by server \n");
        close(sockfd);
        return -1;
    }

    // 发送文件头 - 这里是只传输了一个文件的全部信息， todo 传输多个文件
    if(send_file_header(sockfd, CMD_PUT_FILE, file_stat.st_size, strlen(filename)))
    {
        printf("Failed to send file header \n");
        close(sockfd);
        return -1;       
    }

    // 发送文件名称
    send(sockfd, filename, strlen(filename), 0);

    // 传输数据
    int file_fd = open(filename, O_RDONLY);
    off_t offset = 0;

    printf("Sending file: %s (Size  %ld bytes)\n", filename, (long)file_stat.st_size);

    // todo ： 大文件传输的时候考虑sendfile + 分块 + epoll
    ssize_t sent = sendfile(sockfd, file_fd, &offset, file_stat.st_size);

    if(sent != file_stat.st_size)
    {
        printf("File transfer incomplete: sent %ld/%ld bytes\n", sent, (long)file_stat.st_size);
        close(file_fd);
        close(sockfd); 
        return -1;
    }

    // 等待服务器发送的确认消息， todo:如果是 NAK 就重试N次
    FileHeader response;
    int ret = receive_file_header(sockfd, &response);
    close(file_fd);

    if(ret >= 0)
    {
        if(response.command == CMD_ACK)
        {
            printf("File transfer completed successfully\n");
            close(sockfd);
            return 0;
        }
        else
        {
            printf("File transfer failed (server rejected)\n");
            close(sockfd);
            return -1;
        }
    }
    printf("failed to receive response from server \n");
    close(sockfd);
    return -1;
}


// 从服务器取出文件， 返回 0 表示成功
int receive_tcp_file(const char* filename, const char* ip, int port, const char*username, const char* password)
{
    int sockfd;

    FileHeader header;

    // 连接到服务器
    sockfd = open_clientfd(ip, port);
    if(sockfd < 0)
    {
        return -1;
    }
    printf("Connect to %s:%d\n", ip, port);

    // 发送认证消息
    if(send_auth_request(sockfd, username, password) < 0) 
    {
        printf("Authentication failed \n");
        close(sockfd);
        return -1;
    }

    // 接受认证响应
    if(receive_auth_reponse(sockfd) <= 0)
    {
        printf("Authentication rejected by server \n");
        close(sockfd);
        return -1;
    }

    // 发送文件头 - 这里是只传输了一个文件的全部信息， todo 传输多个文件
    if(send_file_header(sockfd, CMD_GET_FILE, 0, strlen(filename)))
    {
        printf("Failed to send file header \n");
        close(sockfd);
        return -1;       
    }

    // 发送文件名称
    send(sockfd, filename, strlen(filename), 0);

    // 接收文件头 - 包含文件的大小和文件名的大小
    if(receive_file_header(sockfd, &header) < 0)
    {
        printf("Failed to receive file header");
        close(sockfd);
        return -1;
    }

    if(header.command != CMD_GET_FILE)
    {
        printf("Server rejected file request\n");
        close(sockfd);
        return -1;
    }

    // 接收文件名
    char received_filename[MAX_FILENAME_LEN];
    if(recv(sockfd, received_filename, header.filename_len, 0) != header.filename_len)
    {
        printf("Failed to receive filename\n");
        close(sockfd);
        return -1;
    }
    received_filename[header.filename_len] = '\0';

    printf("Receiving file: %s (Size: %u bytes)\n", received_filename, header.filesize);

    // 接收文件内容
    FILE* file = fopen(received_filename, "wb");
    if(!file)
    {
        perror("Failed to create file\n");
        close(sockfd);
        return -1;
    }

    char buffer[65536];
    uint32_t total_received = 0;
    ssize_t bytes_received;
    
    while (total_received < header.filesize) {
        size_t to_receive = sizeof(buffer);
        if (header.filesize - total_received < to_receive) {
            to_receive = header.filesize - total_received;
        }
        
        bytes_received = recv(sockfd, buffer, to_receive, 0);
        if (bytes_received <= 0) {
            send_response(sockfd, CMD_NAK);
            printf("Connection error during file transfer\n");
            fclose(file);
            close(sockfd);
            return -1;
        }
        
        fwrite(buffer, 1, bytes_received, file);
        total_received += bytes_received;
        
        // 显示进度
        if (header.filesize > 0) {
            float progress = (float)total_received / header.filesize * 100;
            printf("\rProgress: %.1f%% (%u/%u bytes)", 
                   progress, total_received, header.filesize);
            fflush(stdout);
        }
    }

    send_response(sockfd, CMD_ACK);
    printf("\nFile received successfully: %s\n", received_filename);
    
    fclose(file);
    close(sockfd);
    return 0;   

}