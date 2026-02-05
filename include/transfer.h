#ifndef _TRANSFER_H_
#define _TRANSFER_H_


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <pthread.h>
#include <stdbool.h>

// 网络头文件
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <ifaddrs.h>

#include <sys/select.h>  // select(), fd_set, FD_*宏
#include <sys/time.h>    // struct timeval
#include <errno.h>       // errno, EINTR等错误码
#include <fcntl.h>  

#include <sys/sendfile.h>  // sendfile()
#include <sys/stat.h>      // stat(), S_ISREG()
#include <strings.h> 
#include <inttypes.h>

// TCP 文件传输相关配置
#define TCP_PORT 5060
#define MAX_PATH_LEN 256
#define MAX_USERNAME_LEN 32
#define MAX_PASSWORD_LEN 32
#define MAX_FILENAME_LEN 64
#define MAX_IP_LEN      32

#define MAGIC_NUMBER 0x4C465450 // LFTP 传输的魔数

// 用户认证信息
typedef struct {
    char username[MAX_USERNAME_LEN];
    char password[MAX_PASSWORD_LEN];
    int authenticated;              // 0 表示不需要验证
} UserAuth;


// 服务器配置信息
typedef struct {
    int is_running;
    int port;
    char root_path[MAX_PATH_LEN];
    UserAuth auth;                  // 认证信息
    int server_fd;                  // 服务器socket
    pthread_t server_thread;        // 服务器线程
} ServerConfig;

// 客户端处理线程参数
typedef struct {
    int client_fd;
    struct sockaddr_in client_addr;
    ServerConfig config;
}ClientThreadArgs;


// 文件传输命令
typedef enum {
    CMD_PUT_FILE = 0x01,    // 上传文件
    CMD_GET_FILE = 0x02,    // 下载文件
    CMD_ACK = 0x03,         // 确认
    CMD_NAK = 0x04,         // 拒绝
} CommandType;

// 文件传输头
typedef struct {
    uint32_t magic;
    uint16_t version;
    uint16_t command;          // 命令类型
    uint32_t filesize;           // 文件大小
    uint16_t filename_len;          // 文件名长度
    uint8_t reserved[16];      // 保留字段
} FileHeader;

// 认证头
typedef struct {
    char username[MAX_USERNAME_LEN];
    char password[MAX_PASSWORD_LEN];
    uint8_t auth_result;    // 0 -表示失败； 1-表示成功
}AuthHeader;

// TCP 服务器相关
int start_tcp_server(int port, const char* root_path, 
                     const char* username, const char* password);
void stop_tcp_server();
void* tcp_server_thread(void* arg);
int handle_client_requests(int client_fd, const char *root_path);
void* handle_client_connection_thread(void* arg);

int handle_file_upload(int client_fd, const char* root_path, const char* filename, uint32_t filesize);
int handle_file_download(int client_fd, const char* root_path, const char* filename);
int validate_path(const char* root_path, const char* requested_path);

// TCP 客户端相关
int send_tcp_file(const char* filename, const char* ip, int port, const char*username, const char* password);
int receive_tcp_file(const char* filename, const char* ip, int port, const char*username, const char* password);


// TCP 相关的命令行解析
int parse_server_command(int argc, char* argv[]);
int parse_transfer_command(int argc, char* argv[]);


// 工具函数
int open_clientfd(const char* ip_address, int port);
int open_listenfd(int port);
int authenticate_client(int client_fd, UserAuth* server_auth);
int send_response(int sockfd, uint16_t command);
int send_auth_response(int sockfd, int success);
int receive_auth_reponse(int sockfd);

int send_file_header(int sockfd, uint16_t command, int filesize, int filename_len);
int receive_file_header(int sockfd, FileHeader* header);
int send_auth_request(int sockfd, const char* username, const char* password);


#endif