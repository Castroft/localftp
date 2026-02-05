// device_manager.c - 设备列表管理
#include "discovery.h"
#include "color.h"


DeviceList *device_list = NULL;
pthread_mutex_t list_mutex = PTHREAD_MUTEX_INITIALIZER;


// 初始化设备列表
void init_device_list() {
    device_list = NULL;
}

// 添加或更新设备
void add_device(const char* name, const char* ip) {
    pthread_mutex_lock(&list_mutex);
    
    DeviceList *current = device_list;
    
    // 查找是否已存在该IP的设备
    while (current != NULL) {
        if (strcmp(current->info.ip_address, ip) == 0) {
            // 更新已有设备
            strncpy(current->info.device_name, name, 63);
            current->info.last_seen = time(NULL);
            current->info.is_online = 1;
            
            pthread_mutex_unlock(&list_mutex);
            return;
        }
        current = current->next;
    }
    
    // 添加新设备
    DeviceList *new_node = (DeviceList*)malloc(sizeof(DeviceList));
    if (new_node == NULL) {
        pthread_mutex_unlock(&list_mutex);
        return;
    }
    
    strncpy(new_node->info.device_name, name, 63);
    strncpy(new_node->info.ip_address, ip, 15);
    new_node->info.last_seen = time(NULL);
    new_node->info.is_online = 1;
    new_node->next = device_list;  // 插入到链表头部
    device_list = new_node;

    pthread_mutex_unlock(&list_mutex);

    printf("[+] New device discovered: %s (%s)\n", name, ip);
}

// 更新设备活跃时间
void update_device(const char* ip) {
    pthread_mutex_lock(&list_mutex);
    
    DeviceList *current = device_list;
    while (current != NULL) {
        if (strcmp(current->info.ip_address, ip) == 0) {
            current->info.last_seen = time(NULL);
            current->info.is_online = 1;
            pthread_mutex_unlock(&list_mutex);
            return;
        }
        current = current->next;
    }
    
    pthread_mutex_unlock(&list_mutex);
}

// 删除设备
void remove_device(const char* ip) {
    pthread_mutex_lock(&list_mutex);
    
    DeviceList **pp = &device_list;
    DeviceList *current = device_list;
    
    while (current != NULL) {
        if (strcmp(current->info.ip_address, ip) == 0) {
            *pp = current->next;
            printf("[-] Device removed: %s (%s)\n", 
                   current->info.device_name, ip);
            free(current);
            pthread_mutex_unlock(&list_mutex);
            return;
        }
        pp = &current->next;
        current = current->next;
    }
    
    pthread_mutex_unlock(&list_mutex);
}

// 清理超时设备
void cleanup_old_devices() {
    time_t now = time(NULL);
    pthread_mutex_lock(&list_mutex);
    
    DeviceList **pp = &device_list;
    DeviceList *current = device_list;
    int removed_count = 0;
    
    while (current != NULL) {
        if (difftime(now, current->info.last_seen) > DEVICE_TIMEOUT) {
            // 标记为离线而不是立即删除
            if (current->info.is_online) {
                printf("[-] Device offline: %s (%s) - timeout\n",
                       current->info.device_name, 
                       current->info.ip_address);
                current->info.is_online = 0;
            }
            
            // 如果离线时间超过2倍超时时间，则删除
            if (difftime(now, current->info.last_seen) > DEVICE_TIMEOUT * 2) {
                *pp = current->next;
                free(current);
                current = *pp;
                removed_count++;
            } else {
                pp = &current->next;
                current = current->next;
            }
        } else {
            pp = &current->next;
            current = current->next;
        }
    }
    
    pthread_mutex_unlock(&list_mutex);

    if (removed_count > 0) {
        printf("[i] Cleaned up %d old devices\n", removed_count);
    }
}

// 获取设备数量
int get_device_count() {
    int count = 0;
    pthread_mutex_lock(&list_mutex);
    
    DeviceList *current = device_list;
    while (current != NULL) {
        if (current->info.is_online) {
            count++;
        }
        current = current->next;
    }
    
    pthread_mutex_unlock(&list_mutex);
    return count;
}

// 打印设备列表
void print_device_list() {
    
    time_t now = time(NULL);
    printf("\n=== Online Devices (%d) ===\n", get_device_count());
    printf("%-20s %-15s %-8s %s\n", "Device Name", "IP Address", "Status", "Last Seen");
    printf("------------------------------------------------------------\n");

    pthread_mutex_lock(&list_mutex);
    DeviceList *current = device_list;
    int online_count = 0;
    
    while (current != NULL) {
        time_t diff = now - current->info.last_seen;
        const char* status = current->info.is_online ? "Online" : "Offline";
        
        if (current->info.is_online) {
            printf("\033[32m");  // 绿色
        } else {
            printf("\033[90m");  // 灰色
        }
        
        printf("%-20s %-15s %-8s %lds ago\033[0m\n",
               current->info.device_name,
               current->info.ip_address,
               status,
               diff);
        
        if (current->info.is_online) {
            online_count++;
        }
        current = current->next;
    }
    pthread_mutex_unlock(&list_mutex);

    if (online_count == 0) {
        printf("No online devices found.\n");
    }
    
    printf("============================================================\n\n");
}