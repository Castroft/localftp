// discovery_threads.c
#include "discovery.h"
#include "color.h"
#include <signal.h>

// 广播发送线程， 每30s发送一次
void* broadcast_sender_thread()
{
    int send_sock = create_send_socket();
    if(send_sock < 0)
    {
        fprintf(stderr, "[Sender] Failed to create send socket\n");
        return NULL;
    }

    char device_name[64];
    char my_ip[32];

    // 获取本机信息
    get_hostname(device_name, sizeof(device_name));
    if(get_local_ip(my_ip, sizeof(my_ip)) < 0)
    {
        strcpy(my_ip, "0.0.0.0");
    }

    printf("[Sender] I am %s (%s)\n", device_name, my_ip);

    char broadcast_msg[256];
    snprintf(broadcast_msg, sizeof(broadcast_msg), "%s|%ld", device_name, time(NULL));

    int count = 0;
    while(running)
    {
        // 发送广播
        if(send_broadcast_message(send_sock, broadcast_msg) > 0)
        {
            count ++;
            printf("[Sender] Heartbeat #%d send\n", count);
        }
        // 等待 30s
        for(int i = 0; i < DISCOVERY_INTERVAL && running; i ++)
        {
            sleep(1);
        }
    }
    close(send_sock);
    printf("[Sender] Thread terminated\n");
    return NULL;
}

// 广播接收线程 - 持续接听
void* broadcast_receiver_thread()
{
    int recv_sock = create_recv_socket();
    if(recv_sock < 0)
    {
        fprintf(stderr, "[Receiver] Failed to create receive socket\n");
        return NULL;
    }

    printf("[Receiver] Listening for broadcast messages ... \n");

    // 使用select实现超时和非阻塞结合的接收
    fd_set read_fds;
    struct timeval timeout;
    int max_fd = recv_sock;

    int recv_count = 0;

    while(running)
    {
        FD_ZERO(&read_fds);
        FD_SET(recv_sock, &read_fds);

        // 设置超时为1s，这样我们可以定期检查running标志
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;

        int activity = select(max_fd + 1, &read_fds, NULL, NULL, &timeout);

        if(activity < 0 && errno != EINTR)
        {
            perror("[Receiver] Select error");
            break;
        }

        if(activity > 0)
        {
            if(FD_ISSET(recv_sock, &read_fds))
            {
                // 有数据可读
                if(receive_broadcast_message(recv_sock) > 0)
                {
                    recv_count++;
                }
            }
        }

        // 每隔 5s 清理一下一次超时设备
        static time_t last_cleanup = 0;
        time_t now = time(NULL);
        if(now - last_cleanup >= 5)
        {
            cleanup_old_devices();
            last_cleanup = now;
        }
    }

    close(recv_sock);
    printf("[Receiver] Thread terminated. Received %d messages.\n", recv_count);
    return NULL;
}

// 启动发现系统(创建两个线程)
void start_discovery_system()
{
    pthread_t sender_tid, receiver_tid;

    init_device_list();
    // 创建发送线程
    if(pthread_create(&sender_tid, NULL, broadcast_sender_thread, NULL) != 0)
    {
        fprintf(stderr, "Failed to create sender thread\n");
        return;
    }

    // 创建接收线程
    if(pthread_create(&receiver_tid, NULL, broadcast_receiver_thread, NULL) != 0)
    {
        fprintf(stderr, "Failed to create receiver thread\n");
        running = 0;
        pthread_join(sender_tid, NULL);
        return;
    }

    printf("[Discovery] Discovery system started (sender + receiver threads)\n");
    
    // 分离线程， 让系统回收
    pthread_detach(sender_tid);
    pthread_detach(receiver_tid);
}