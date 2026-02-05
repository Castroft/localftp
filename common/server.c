// server.c
#include "discovery.h"
#include "transfer.h"
#include <sys/stat.h>
#include <dirent.h>
#include <fcntl.h>
#include <libgen.h>

// 全局服务器配置
static ServerConfig server_config = {0};
static pthread_mutex_t server_mutex = PTHREAD_MUTEX_INITIALIZER; 
// 这个线程锁是为了控制启动TCP和终止TCP直接不能相互干扰， 在进程中间可以直接使用config， 不用加线程锁

// 启动 tcp server 服务器 - 返回 0 表示启动成功
int start_tcp_server(int port, const char* root_path, const char* username, const char* password)
{
    pthread_mutex_lock(&server_mutex);

    if(server_config.is_running) {
        printf("Server is already running on port %d\n", server_config.port);
        pthread_mutex_unlock(&server_mutex);
        return -1;
    }

    // 初始化服务器配置
    server_config.port = port > 0 ? port : TCP_PORT;
    server_config.is_running = 0;

    // 设置根路径
    if(root_path && strlen(root_path) > 0)
    {
        strncpy(server_config.root_path, root_path, MAX_PATH_LEN -1);

        // 确保文件夹路径存在
        struct stat st;
        if(stat(server_config.root_path, &st) != 0 || !S_ISDIR(st.st_mode))
        {
            printf("Error: Root path '%s' does not exist or is not a directory\n", root_path);
            pthread_mutex_unlock(&server_mutex);
            return -1;           
        }
    }
    else
    {
        getcwd(server_config.root_path, MAX_PATH_LEN - 1);
        server_config.root_path[MAX_PATH_LEN - 1] = '\0';
    }

    // 设置认证信息
    if(username && strlen(username) > 0)
    {
        strncpy(server_config.auth.username, username, MAX_USERNAME_LEN - 1);
        server_config.auth.username[MAX_USERNAME_LEN - 1] = '\0';

        if (password) {
            strncpy(server_config.auth.password, password, MAX_PASSWORD_LEN - 1);
            server_config.auth.password[MAX_PASSWORD_LEN - 1] = '\0';
        } else {
            server_config.auth.password[0] = '\0';  // 空密码
        }
        server_config.auth.authenticated = 1;
    }
    else{
        server_config.auth.username[0] = '\0';
        server_config.auth.password[0] = '\0';
        server_config.auth.authenticated = 0;
    }

    printf("Starting TCP server on port %d\n", server_config.port);
    printf("Root path: %s\n", server_config.root_path);

    // 创建服务器线程与shell进程并行运行， 可以输入stop
    if(pthread_create(&server_config.server_thread, NULL, tcp_server_thread, &server_config) != 0) {
        perror("Failed to create server thread");
        pthread_mutex_unlock(&server_mutex);
        return -1;       
    }

    server_config.is_running = 1;
    pthread_mutex_unlock(&server_mutex);
    return 0;
}

// tcp 服务器线程
void* tcp_server_thread(void* arg)
{
    ServerConfig* config = (ServerConfig*)arg;
    int listenfd, connfd;
    int port = config->port;
    socklen_t client_len = sizeof(struct sockaddr_in);

    struct sockaddr_in client_addr;

    listenfd = open_listenfd(port);
    if (listenfd < 0) {
        printf("Failed to create listening socket\n");
        return NULL;
    }
    config->server_fd = listenfd;
    
    printf("TCP server listening on port %d\n", config->port);
    printf("Ready to accept connections...\n");


    while(config->is_running)
    {
        fd_set read_fds;
        struct timeval timeout;

        FD_ZERO(&read_fds);
        FD_SET(listenfd, &read_fds);

        timeout.tv_sec = 1;
        timeout.tv_usec = 0;

        int activity = select(listenfd + 1, &read_fds, NULL, NULL, &timeout);

        if(activity < 0 && errno != EINTR)
        {
            perror("Select error\n");
            break;
        }

        if(activity > 0 && FD_ISSET(listenfd, &read_fds))
        {
            // 接收新的连接
            connfd = accept(listenfd, (struct sockaddr*)&client_addr, &client_len);
            if(connfd < 0)
            {
                perror("Accept failed\n");
                continue;
            }

            printf("New connection from %s:%d\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));

            pthread_t client_thread;
            ClientThreadArgs *args = malloc(sizeof(ClientThreadArgs));

            if(args)
            {
                args->client_fd = connfd;
                memcpy(&args->client_addr, &client_addr, sizeof(client_addr));
                memcpy(&args->config, config, sizeof(ServerConfig));

                pthread_create(&client_thread, NULL, handle_client_connection_thread, args);
                pthread_detach(client_thread);

            } else 
            {
                close(connfd);
            }
        }
    }


    // 关闭监听socket
    close(listenfd);
    config->server_fd = -1;

    printf("TCP Server thread terminated! \n");
    return NULL;
}


void stop_tcp_server()
{
    pthread_mutex_lock(&server_mutex);

    if(!server_config.is_running)
    {
        pthread_mutex_unlock(&server_mutex);
        return;
    }

    server_config.is_running = 0;
    pthread_join(server_config.server_thread, NULL);

    // 关闭服务器socket（如果有）
    if (server_config.server_fd > 0) {
        close(server_config.server_fd);
        server_config.server_fd = -1;
    }
    printf("TCP server stopped\n");

    pthread_mutex_unlock(&server_mutex);
}

void* handle_client_connection_thread(void* arg)
{
    ClientThreadArgs* args = (ClientThreadArgs*)arg;
    int clientfd = args->client_fd;
    struct sockaddr_in client_addr = args->client_addr;
    ServerConfig config = args->config;


    free(args);

    if(authenticate_client(clientfd, &config.auth) == 0)
    {
        handle_client_requests(clientfd, config.root_path);
    }
    close(clientfd);
    printf("Connection closed for %s:%d\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
    return NULL;
}

int authenticate_client(int client_fd, UserAuth* server_auth)
{
    AuthHeader auth_header;
    ssize_t bytes;

    bytes = recv(client_fd, &auth_header, sizeof(AuthHeader), 0);
    if(bytes != sizeof(AuthHeader))
    {
        printf("Failed to receive auth header\n");
        send_auth_response(client_fd, 0);  // 认证失败
        return -1;    
    }

    if(server_auth->authenticated)
    {
        if(strcmp(server_auth->username, auth_header.username) == 0 &&
            strcmp(server_auth->password, auth_header.password) == 0) {
                send_auth_response(client_fd, 1);
                return 0;
            }else
            {
                send_auth_response(client_fd, 0);
                return -1;
            }
    }else{
        send_auth_response(client_fd, 1);
        return 0;
    }
}


// 处理客户端请求
int handle_client_requests(int client_fd, const char *root_path)
{
    FileHeader header;
    while(1)
    {
        // 接收文件头
        if(receive_file_header(client_fd, &header) < 0) {
            printf("failed to receive file header\n");
            break;
        }

        //  验证魔数
        if(header.magic != MAGIC_NUMBER) {
            printf("Invailed magic number\n");
            break;
        }

        // 根据命令类型来处理
        switch (header.command)
        {
            case CMD_PUT_FILE :{
                printf("Client put file\n");
                
                // 接收文件名
                char filename[MAX_FILENAME_LEN];
                if(recv(client_fd, filename, header.filename_len, 0) != header.filename_len)
                {
                    printf("Failed to receive filename\n");
                    send_response(client_fd, CMD_NAK);
                    break;
                }
                filename[header.filename_len] = '\0';

                printf("Receiving file: %s (Size: %u bytes)\n", filename, header.filesize);

                // 处理文件上传
                if(handle_file_upload(client_fd, root_path, filename, header.filesize) == 0)
                {
                    printf("File received successfully: %s\n", filename);
                    send_response(client_fd, CMD_ACK);
                }
                else
                {
                    printf("Failed to receive file %s\n", filename);
                    send_response(client_fd, CMD_NAK);
                }
                break;
            }
            case CMD_GET_FILE :{
                printf("Client get file\n");
                
                char filename[MAX_FILENAME_LEN];
                if(recv(client_fd, filename, header.filename_len, 0) != header.filename_len)
                {
                    printf("Failed to receive filename\n");
                    send_response(client_fd, CMD_NAK);
                    break;
                }
                filename[header.filename_len] = '\0';

                printf("sending file: %s", filename);

                // 处理文件下载
                if(handle_file_download(client_fd, root_path, filename) == 0)
                {
                    printf("File sent successfully: %s\n", filename);
                }
                else
                {
                    send_file_header(client_fd, CMD_NAK, 0, 0);
                    printf("Failed to send file: %s", filename);
                }
                break;
            }
            default:{
                printf("Unknown command: %d\n", header.command);
                send_response(client_fd, CMD_NAK);
                break;
            }
        }
    }
    return 0;
}