// discovery.h 中添加/修改
#ifndef _DISCOVERY_H_
#define _DISCOVERY_H_


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

// 定义常量
#define BROADCAST_PORT 5050
#define DISCOVERY_INTERVAL 30  // 发送间隔30秒
#define DEVICE_TIMEOUT 90      // 设备超时时间
#define MAX_DEVICES 50         // 最大设备数量

// 设备信息结构
typedef struct {
    char device_name[64];
    char ip_address[32];
    time_t last_seen;          // 最后活跃时间
    int is_online;            // 是否在线
} DeviceInfo;

// 设备列表
typedef struct DeviceNode {
    DeviceInfo info;
    struct DeviceNode *next;
} DeviceList;

// 全局变量声明
extern DeviceList *device_list;
extern pthread_mutex_t list_mutex;
extern int running;

// 函数声明
// 网络相关
int create_send_socket();
int create_recv_socket();
int send_broadcast_message(int send_sock, const char* message);
int receive_broadcast_message(int recv_sock);

// 设备管理
void init_device_list();
void add_device(const char* name, const char* ip);
void update_device(const char* ip);
void remove_device(const char* ip);
void cleanup_old_devices();
void print_device_list();
int get_device_count();

// 工具函数
int get_local_ip(char *buffer, size_t buflen);
int get_hostname(char *buffer, size_t buflen);

// 线程函数
void* broadcast_sender_thread();
void* broadcast_receiver_thread();
void start_discovery_system();

#endif