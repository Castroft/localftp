#ifndef _SHELL_H_
#define _SHELL_H_

// shell 相关头文件
#include <dirent.h>
#include <termios.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <limits.h>

#define PATH_MAX 4096

void set_terminal_raw_mode(int enable);
int read_input(char *buffer);
int execute_pwd(char *cwd, size_t size);
void execute_command(char *input);


#endif