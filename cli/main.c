// main.c
#include "color.h"
#include "discovery.h"
#include "shell.h"
#include <pthread.h>
#include <signal.h>


// # 在终端抓包查看广播流量
// sudo tcpdump -i any port 5050 -X

int running = 1;
bool server_mode = false;
pthread_t server_thread;

void signal_handler(int sig)
{
    printf("\n shutting down ... \n");
    running = 0;
    server_mode = false;
}

int main() {
    char input[MAX_INPUT];
    char cwd[PATH_MAX];

    if(execute_pwd(cwd, sizeof(cwd)))
    {
        return -1;
    }

    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    //  启动发现系统
    start_discovery_system();

    // 启动 TCP 服务器端口，接收客户端的连接
    // start_transfer_server();
    sleep(2);

    printf(COLOR_GREEN"=== Unix/Linux Local ftp Shell ===\n"COLOR_RESET);
    printf(COLOR_GREEN"Enter 'help' Display command help \n"COLOR_RESET);
    printf(COLOR_GREEN"Enter 'exit' Exit lftp\n\n"COLOR_RESET);
    
    while (1) {
        printf(COLOR_GREEN"(lftp)%s > "COLOR_RESET, cwd);
        fflush(stdout);

        if (read_input(input) > 0) {
            execute_command(input);
        }
    }
    
    return 0;
}