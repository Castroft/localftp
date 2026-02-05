// network.c - 分离的广播发送和接收
#include "discovery.h"
#include "color.h"
#include <errno.h>
#include <sys/time.h>
#include <signal.h>


// 创建发送socket（不绑定端口）
int create_send_socket() {
    int sockfd;
    int broadcast_enable = 1;
    
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("[Sender] socket creation failed");
        return -1;
    }
    
    // 设置广播选项
    if (setsockopt(sockfd, SOL_SOCKET, SO_BROADCAST,
                   &broadcast_enable, sizeof(broadcast_enable)) < 0) {
        perror("[Sender] setsockopt SO_BROADCAST failed");
        close(sockfd);
        return -1;
    }
    
    // 设置发送缓冲区大小
    int sendbuf = 65536;
    setsockopt(sockfd, SOL_SOCKET, SO_SNDBUF, &sendbuf, sizeof(sendbuf));
    
    printf("[Sender] Broadcast send socket created\n");
    return sockfd;
}

// 创建接收socket（绑定到5050端口）
int create_recv_socket() {
    int sockfd;
    struct sockaddr_in addr;
    int reuse = 1;
    
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("[Receiver] socket creation failed");
        return -1;
    }
    
    // 设置地址重用（允许多个程序监听同一端口）
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR,
                   &reuse, sizeof(reuse)) < 0) {
        perror("[Receiver] setsockopt SO_REUSEADDR failed");
        // 继续执行，不是致命错误
    }
    
    // 绑定到指定端口
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(BROADCAST_PORT);  //将host小端转为网络字节序大端
    addr.sin_addr.s_addr = INADDR_ANY; // 监听本地所有IP
    // INADDR_ANY (0.0.0.0) 
    // 外部IP：公网IP实际上无法从外部直接监听， 只能监听到达本机的数据
    // 接受发送本机任一IP的5050端口数据，无论数据是从外部网络还是本机内部外送的
    
    if (bind(sockfd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("[Receiver] bind failed");
        close(sockfd);
        return -1;
    }
    
    // 设置接收缓冲区大小
    int recvbuf = 65536;
    setsockopt(sockfd, SOL_SOCKET, SO_RCVBUF, &recvbuf, sizeof(recvbuf));
    
    // 设置为非阻塞模式（可选）
    int flags = fcntl(sockfd, F_GETFL, 0);
    // 获取当前文件状态标志
    fcntl(sockfd, F_SETFL, flags | O_NONBLOCK);
    // 添加非阻塞标志
    // 阻塞标志（默认）：recv() accept() 等函数会一直等待直到 有数据/连接
    // 非阻塞标志： 没有数据，会立即返回错误。
    // 适合需要同时处理多个连接或者轮询的场景
    // 配合 select（） poll（） epoll()
    
    printf("[Receiver] Broadcast receive socket created on port %d\n", BROADCAST_PORT);
    return sockfd;
}

// 发送广播消息
int send_broadcast_message(int send_sock, const char* message) {
    struct sockaddr_in broadcast_addr;
    int ret;
    
    memset(&broadcast_addr, 0, sizeof(broadcast_addr));
    broadcast_addr.sin_family = AF_INET;
    broadcast_addr.sin_port = htons(BROADCAST_PORT);
    broadcast_addr.sin_addr.s_addr = htonl(INADDR_BROADCAST);
    
    ret = sendto(send_sock, message, strlen(message), 0,
                 (struct sockaddr*)&broadcast_addr,
                 sizeof(broadcast_addr));
    
    if (ret < 0) {
        perror("[Sender] sendto broadcast failed");
    } else {
        printf("[Sender] Broadcast sent: %s\n", message);
    }
    
    return ret;
}

// 接收广播消息（阻塞版本）
int receive_broadcast_message(int recv_sock) {
    char buffer[1024];
    struct sockaddr_in sender_addr;
    socklen_t addr_len = sizeof(sender_addr);
    char sender_ip[INET_ADDRSTRLEN];
    
    // 接收数据
    int bytes_received = recvfrom(recv_sock, buffer, sizeof(buffer)-1, 0,
                                  (struct sockaddr*)&sender_addr, &addr_len);
    
    if (bytes_received > 0) {
        buffer[bytes_received] = '\0';
        
        // 转换IP地址
        inet_ntop(AF_INET, &sender_addr.sin_addr, sender_ip, sizeof(sender_ip));
        
        // 不接收自己发送的消息
        char my_ip[32];
        if (get_local_ip(my_ip, sizeof(my_ip)) == 0 &&
            strcmp(sender_ip, my_ip) == 0) {
            return 0;  // 忽略自己
        }
        
        printf("[Receiver] Received from %s: %s\n", sender_ip, buffer);
        
        // 解析消息格式：DEVICE_NAME|TIMESTAMP
        char device_name[64];
        
        if (sscanf(buffer, "%[^|]|", device_name) == 1) {
            add_device(device_name, sender_ip);
            return 1;  // 成功接收并处理
        }
    } else if (bytes_received < 0) {
        // 非阻塞socket会返回EAGAIN或EWOULDBLOCK
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
            perror("[Receiver] recvfrom error");
        }
    }
    
    return bytes_received;
}

/*
应用层缓冲区（buffer[1024]）
系统调用recvfrom（）
内核接收缓冲区（recvbuf = 65536）
网卡硬件缓冲区
网络线路
*/

// 获取本机IP地址
int get_local_ip(char *buffer, size_t buflen) {
    struct ifaddrs *ifaddr, *ifa;
    int found = 0;
    
    if (getifaddrs(&ifaddr) == -1) {
        return -1;
    }
    
    for (ifa = ifaddr; ifa != NULL && !found; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr == NULL) continue;
        
        // 只关注IPv4
        if (ifa->ifa_addr->sa_family == AF_INET) {
            struct sockaddr_in *sa = (struct sockaddr_in *)ifa->ifa_addr;
            const char* addr = inet_ntoa(sa->sin_addr);
            
            // 跳过回环地址和docker等虚拟接口
            if (strcmp(ifa->ifa_name, "lo") != 0 &&
                strncmp(ifa->ifa_name, "docker", 6) != 0 &&
                strncmp(ifa->ifa_name, "br-", 3) != 0 &&
                strncmp(ifa->ifa_name, "veth", 4) != 0 &&
                strncmp(addr, "172.", 4) != 0 &&  // Docker网段
                strncmp(addr, "169.254.", 8) != 0 &&  // 链路本地地址
                strcmp(addr, "127.0.0.1") != 0) {
                
                strncpy(buffer, addr, buflen - 1);
                buffer[buflen - 1] = '\0';
                found = 1;
            }
        }
    }
    
    freeifaddrs(ifaddr);
    return found ? 0 : -1;
}

// 获取主机名
int get_hostname(char *buffer, size_t buflen) {
    if (gethostname(buffer, buflen - 1) < 0) {
        strcpy(buffer, "unknown");
        return -1;
    }
    buffer[buflen - 1] = '\0';
    return 0;
}