// utils.c
#include "discovery.h"
#include "color.h"
#include "transfer.h"

typedef struct sockaddr SA;

int open_clientfd(const char* ip_address, int port)
{
    int client_fd;
    struct sockaddr_in server_addr;

    if((client_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        return -1;
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(ip_address);  // ip_address 需要保证是IPV4
    server_addr.sin_port = htons((unsigned short)port);

    if(connect(client_fd, (SA*)&server_addr,  sizeof(server_addr)) < 0)
    {
        perror("open_clientfd: Connection failed");
        close(client_fd);
        return -1;
    }
    return client_fd;
}

int open_listenfd(int port)
{
    int listenfd, optval = 1;
    struct sockaddr_in serveraddr;
    
    // 创建 socket 描述符
    if((listenfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
        return -1;


    // 设置 socket
    if (setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, (const void*)&optval, sizeof(int)) < 0)
    {
        return -1;
    }

    // 将 listenfd 设置为 请求该设备所有IP 的端点
    bzero((char*)&serveraddr, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
    serveraddr.sin_port = htons((unsigned short)port);

    if(bind(listenfd, (SA*)&serveraddr, sizeof(serveraddr)) < 0)
        return -1;

    // 将 listening socket 设置为准备接收所有的连接请求
    if(listen(listenfd, 5) < 0)
        return -1;

    return listenfd;
}


int send_file_header(int sockfd, uint16_t command, int filesize, int filename_len)
{
    FileHeader header;
    memset(&header, 0, sizeof(FileHeader));

    header.magic = MAGIC_NUMBER;
    header.version = 1;
    header.command = command;
    header.filesize = filesize;
    header.filename_len = filename_len;

    // 转化为网络字节序
    header.magic = htonl(header.magic);
    header.version = htons(header.version);
    header.command = htons(header.command);
    header.filesize = htonl(header.filesize);
    header.filename_len = htons(header.filename_len);

    ssize_t sent =  send(sockfd, &header, sizeof(FileHeader), 0);
    return (sent == sizeof(FileHeader)) ? 0 : -1;
}

// 接收文件头
int receive_file_header(int sockfd, FileHeader* header)
{
    if (recv(sockfd, header, sizeof(FileHeader), 0) != sizeof(FileHeader)) {
        return -1;
    }

    header->magic = ntohl(header->magic);
    header->version = ntohs(header->version);
    header->command = ntohs(header->command);
    header->filesize = ntohl(header->filesize);
    header->filename_len = ntohs(header->filename_len);

    return 0;
}


// 发送认证请求
int send_auth_request(int sockfd, const char* username, const char* password)
{
    AuthHeader request;
    memset(&request, 0, sizeof(AuthHeader));

    if(username)
    {
        strncpy(request.username, username, MAX_USERNAME_LEN - 1);
        request.username[MAX_USERNAME_LEN - 1] = '\0';
    }
    if(password)
    {
        strncpy(request.password, password, MAX_PASSWORD_LEN - 1);
        request.password[MAX_PASSWORD_LEN - 1] = '\0';
    }

    return send(sockfd, &request, sizeof(AuthHeader), 0);
}


// 发送认证响应
int send_auth_response(int sockfd, int success)
{
    AuthHeader response;
    memset(&response, 0, sizeof(AuthHeader));
    response.auth_result = success ? 1: 0;      // 1 表示成功， 0 表示失败

    return send(sockfd, &response, sizeof(AuthHeader), 0);
}

// 接受认证响应
int receive_auth_reponse(int sockfd)
{
    AuthHeader response;

    if(recv(sockfd, &response, sizeof(AuthHeader), 0) != sizeof(AuthHeader))
    {
        return -1;
    }
    return response.auth_result;
}

// 发送响应
int send_response(int sockfd, uint16_t command)
{
    FileHeader response;
    memset(&response, 0, sizeof(FileHeader));

    response.magic = MAGIC_NUMBER;
    response.version = 1;
    response.command = command;

    // 必须转换为网络字节序！
    response.magic = htonl(response.magic);
    response.version = htons(response.version);
    response.command = htons(response.command);

    
    ssize_t sent = send(sockfd, &response, sizeof(FileHeader), 0);

    return (sent == sizeof(FileHeader)) ? 0 : -1;
}

// 处理文件上传, 将 put 上传的文件保存在服务器
int handle_file_upload(int client_fd, const char* root_path, const char* filename, uint32_t filesize)
{
    char fullpath[MAX_PATH_LEN * 2];
    struct stat file_stat;

    snprintf(fullpath, sizeof(fullpath), "%s/%s", root_path, filename);

    // 安全验证： 防止路径遍历攻击
    if(validate_path(root_path, fullpath))
    {
        printf("Security violation: Invalid file path\n");
        return -1;
    }

    // 打开文件
    FILE* file = fopen(fullpath, "wb");
    if(!file)
    {
        perror("Failed to open file for writing");
        return -1;
    }

    char buffer[65536];
    uint32_t total_received = 0;
    ssize_t bytes_received;

    printf("saving to: %s\n", fullpath);

    while(total_received < filesize)
    {
        size_t to_receive = sizeof(buffer);
        if(filesize - total_received < to_receive)
        {
            to_receive = filesize - total_received;
        }

        bytes_received = recv(client_fd, buffer, to_receive, 0);

        if(bytes_received <= 0)
        {
            printf("Connection error during file transfer\n");
            fclose(file);
            return -1;
        }
        fwrite(buffer, 1, bytes_received, file);
        total_received += bytes_received;

        if(filesize > 0) {
            float progress = (float)total_received / filesize * 100;
            printf("\rProgress: %.1f%% (%u / %u bytes)", progress, total_received, filesize);
            fflush(stdout);
        }
    } 
    printf("\n");
    fclose(file);
    return 0;
}

int handle_file_download(int client_fd, const char* root_path, const char* filename)
{
    char fullpath[MAX_PATH_LEN * 2];
    struct stat file_stat;

    snprintf(fullpath, sizeof(fullpath), "%s/%s", root_path, filename);

    //  安全验证
    if(validate_path(root_path, fullpath))
    {
        printf("Security violation: Invaild file path\n");
        return -1;
    }

    // 检查文件存在
    if(stat(fullpath, &file_stat) != 0)
    {
        printf("File not found: %s\n", fullpath);
        return -1;
    }

    // 检查文件格式是否正确
    if(!S_ISREG(file_stat.st_mode))
    {
        printf("Not a regular file :%s\n", fullpath);
        return -1;
    }


    if(send_file_header(client_fd, CMD_GET_FILE, file_stat.st_size, strlen(filename)) < 0)
    {
        printf("Failed to send file header \n");
        return -1;
    }

    send(client_fd, filename, strlen(filename), 0);

    // 传输数据
    int file_fd = open(fullpath, O_RDONLY);
    off_t offset = 0;

    printf("Sending file: %s (Size  %ld bytes)\n", filename, (long)file_stat.st_size);
    
    ssize_t sent = sendfile(client_fd, file_fd, &offset, file_stat.st_size);

    if(sent != file_stat.st_size)
    {
        printf("File transfer incomplete: sent %ld/%ld bytes\n", sent, (long)file_stat.st_size);
        close(file_fd);
        close(client_fd); 
        return -1;
    }

    // 等待客户端发送的确认消息， todo:如果是 NAK 就重试N次
    FileHeader response;
    int ret = receive_file_header(client_fd, &response);
    close(file_fd);

    if(ret >= 0 && response.command == CMD_ACK)
    {
        return 0;
    }
    return -1;
}


// 验证路径安全性(防止目录遍历攻击)
int validate_path(const char* root_path, const char* requested_path)
{
    // todo
    return 0;
}