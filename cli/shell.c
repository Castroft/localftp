// shell.c - Unix/Linux专用版本
#include "shell.h"
#include "discovery.h"
#include "transfer.h"
#include "color.h"


// Unix专用的终端设置
void set_terminal_raw_mode(int enable) {
    static struct termios oldt, newt;
    
    if (enable) {
        tcgetattr(STDIN_FILENO, &oldt);
        newt = oldt;
        newt.c_lflag &= ~(ICANON | ECHO | ECHOE | ECHOK | ECHONL);
        tcsetattr(STDIN_FILENO, TCSANOW, &newt);
    } else {
        tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    }
}

// Unix专用的输入读取
int read_input(char *buffer) {
    int pos = 0;
    char ch;
    
    set_terminal_raw_mode(1);
    
    while (1) {
        ch = getchar();
        
        if (ch == '\n' || ch == '\r') {
            printf("\n");
            break;
        }
        else if (ch == 127 || ch == 8) { // Unix退格键
            if (pos > 0) {
                pos--;
                printf("\b \b");
                fflush(stdout);
            }
        }
        else if (ch >= 32 && ch <= 126 && pos < MAX_INPUT - 1) {
            buffer[pos++] = ch;
            putchar(ch);
            fflush(stdout);
        }
        else if (ch == 3) { // Ctrl+C
            set_terminal_raw_mode(0);
            printf("\n");
            exit(0);
        }
    }
    
    buffer[pos] = '\0';
    set_terminal_raw_mode(0);
    return pos;
}

int execute_pwd(char *cwd, size_t size)
{
    if(getcwd(cwd, size) != NULL)
    {
        return 0;
    }
    perror("getcwd");
    return -1;
}



void print_help() {
    printf(COLOR_MAGENTA"\n=== LFTP Commands ===\n\n"COLOR_RESET);
    printf(COLOR_MAGENTA"Discovery:\n"COLOR_RESET);
    printf("  list users    - Show online devices\n");
    printf(COLOR_MAGENTA"\nServer Mode:\n"COLOR_RESET);
    printf("  server [-u user] [-p pass] [-r path] [-P port]  - Start TCP server\n");
    printf("  stop                                            - Stop TCP server\n");
    printf(COLOR_MAGENTA"\nFile Transfer:\n"COLOR_RESET);
    printf("  put <IP> [-u user] [-p pass] <file>  - Upload file to server\n");
    printf("  get <IP> [-u user] [-p pass] <file>  - Download file from server\n");
    printf(COLOR_MAGENTA"\nGeneral:\n"COLOR_RESET);
    printf("  help          - Show this help\n");
    printf("  exit          - Exit program\n");
    printf("  clear         - Clear screen\n");
    printf(COLOR_MAGENTA"================================\n\n"COLOR_RESET);
}

// Unix专用的命令执行
void execute_command(char *input) {
    char *args[64];
    int i = 0;
    
    // 解析命令和参数
    char *token = strtok(input, " ");
    while (token != NULL && i < 63) {
        args[i++] = token;
        token = strtok(NULL, " ");
    }
    args[i] = NULL;
    
    if (i == 0) return;
    
    // 内置命令
    if(strcmp(args[0], "help") == 0) {
        print_help();
    }
    else if (strcmp(args[0], "exit") == 0) {
        printf(COLOR_GREEN"Exit lftp\n"COLOR_RESET);
        exit(0);
    }
    else if (strcmp(args[0], "clear") == 0) {
        printf("\033[2J\033[1;1H"); // Unix清屏
    }
    else if (strcmp(args[0], "list") == 0 && strcmp(args[1], "users") == 0) {
        print_device_list();
    }
    else if (strcmp(args[0], "put") == 0 || strcmp(args[0], "get") == 0)
    {
        // 传输文件
        if(parse_transfer_command(i, args) != 0)
        {
            printf(COLOR_RED"Failed to transfer file\n"COLOR_RESET);
        }
    }
    else if (strcmp(args[0], "server") == 0) {
        // 启动服务器
        if(parse_server_command(i, args) != 0)
        {
            printf(COLOR_RED"Failed to start server\n"COLOR_RESET);
        }
    }
    else if (strcmp(args[0], "stop") == 0) {
        stop_tcp_server();
    }
    else {
        // 外部命令
        pid_t pid = fork();
        if (pid == 0) {
            execvp(args[0], args);
            perror(args[0]);
            exit(1);
        } else if (pid > 0) {
            wait(NULL);
        }
    }
}