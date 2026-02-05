// cmd_parser.c
#include "discovery.h"
#include "color.h"
#include "transfer.h"


// 解析服务器命令
// 格式： server [-u username] [-p password] [-r root_path] [-P port]
int parse_server_command(int argc, char* argv[])
{
    int port = TCP_PORT;
    char *root_path = NULL;
    char *username = NULL;
    char *password = NULL;   

    int i = 1;
    while(i < argc)
    {
        if(strcmp(argv[i], "-u") == 0 && i + 1 < argc) {
            username = argv[++i];
        } else if(strcmp(argv[i], "-p") == 0 && i + 1 < argc) {
            password = argv[++i];
        } else if(strcmp(argv[i], "-r") == 0 && i + 1 < argc) {
            root_path = argv[++i];
        } else if(strcmp(argv[i], "-P") == 0 && i + 1 < argc) {
            int port = atoi(argv[++i]);
            if(port <= 0 || port > 65535) 
            {
                printf("Invalid port number: %d\n", port);
                return -1;                
            }
        }
        else if (strcmp(argv[i], "-h") == 0 || 
                 strcmp(argv[i], "--help") == 0) {
            printf("Usage: server [-u username] [-p password] [-r root_path] [-P port]\n");
            printf("Options:\n");
            printf("  -u username  Set username for authentication\n");
            printf("  -p password  Set password for authentication\n");
            printf("  -r path      Set root directory for file access\n");
            printf("  -P port      Set TCP port (default: %d)\n", TCP_PORT);
            printf("  -h, --help   Show this help message\n");
            return 0;  // 帮助信息，不启动服务器
        }
        else {
            printf("Unknown option: %s\n", argv[i]);
            printf("Use 'server --help' for usage information\n");
            return -1;
        }

        i ++;
    }

    return start_tcp_server(port, root_path, username, password);
}

// 解析文件传输命令, 返回 0 表示成功
// 格式：get/put <IP> [-u username] [-p password] [filename]
int parse_transfer_command(int argc, char* argv[])
{
    char filename[MAX_FILENAME_LEN] = {0};
    char username[MAX_USERNAME_LEN] = {0};
    char password[MAX_PASSWORD_LEN] = {0};
    char ip[MAX_IP_LEN] = {0};

    int i = 1, cmd_type = 0; // cmd_type = 0 表示put， 1 表示get
    int ret = 0;
    if(strcmp(argv[0], "get") == 0)
        cmd_type = 1;
    else if(strcmp(argv[0], "put") == 0)
        cmd_type = 0;
    else
        return -1;

    strncpy(ip, argv[i++], MAX_IP_LEN - 1);
    ip[MAX_IP_LEN - 1] = '\0';

    
    while(i < argc)
    {
        if(strcmp(argv[i], "-u") == 0 && i + 1 < argc) {
            strncpy(username, argv[++i], MAX_USERNAME_LEN - 1);
            username[MAX_USERNAME_LEN - 1] = '\0';
        } else if(strcmp(argv[i], "-p") == 0 && i + 1 < argc) {
            strncpy(password, argv[++i], MAX_PASSWORD_LEN - 1);
            username[MAX_PASSWORD_LEN - 1] = '\0';
        } else if(argv[i][0] != '-')
        {
            strncpy(filename, argv[i], MAX_FILENAME_LEN - 1);
            filename[MAX_FILENAME_LEN - 1] = '\0';
        } else {
            printf("Unknown option: %s\n", argv[i]);
            return -1;
        }
        i ++;
    }

    if(filename[0] == '\0') {
        printf("Missing filename\n");
        return -1;
    }

    if(cmd_type == 0)
    {
        // put 逻辑
        ret = send_tcp_file(filename, ip, TCP_PORT, username, password);
    } else {
        // get 逻辑
        ret = receive_tcp_file(filename, ip, TCP_PORT, username, password);
    }

    return ret;
}