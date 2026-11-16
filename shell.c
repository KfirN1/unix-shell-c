#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#include "stdio.h"
#include "errno.h"
#include "stdlib.h"
#include "unistd.h"
#include <string.h>
#include <signal.h>

#define MAX_COMMANDS 10

int last_status = 0;
char current_prompt[1024] = "hello: ";
char last_command[1024];

/// @brief Redirection IO
/// @param redirect type of redirection (>, >>, <, 2>)
/// @param outfile name of file
void handle_redirection(int redirect, char *outfile)
{
    int fd;
    // > Redirect standard input to outfile overwriting the old file.
    if (redirect == 1)
    {
        fd = open(outfile, O_WRONLY | O_CREAT | O_TRUNC, 0660);
        if (fd < 0)
        {
            perror("File open failed");
            exit(1);
        }
        dup2(fd, STDOUT_FILENO);
        close(fd);
    }

    // >> Redirect standard input to outfile appending to the old file.
    if (redirect == 2)
    {
        fd = open(outfile, O_WRONLY | O_CREAT | O_APPEND, 0660);
        if (fd < 0)
        {
            perror("File open failed");
            exit(1);
        }
        dup2(fd, STDOUT_FILENO);
        close(fd);
    }

    // < Redirect standard output from outfile read only.
    if (redirect == 3)
    {
        fd = open(outfile, O_RDONLY);
        if (fd < 0)
        {
            perror("File open failed");
            exit(1);
        }
        dup2(fd, STDIN_FILENO);
        close(fd);
    }

    // < Redirect standard error.
    if (redirect == 4)
    {
        fd = open(outfile, O_WRONLY | O_CREAT | O_TRUNC, 0660);
        if (fd < 0)
        {
            perror("File open failed");
            exit(1);
        }
        dup2(fd, STDERR_FILENO);
        close(fd);
    }
}

/// @brief Scans a command string for redirection operators, separates the filename, and applies redirection.
/// @param cmd_str The command string (modified in-place to remove the redirection part).
/// @param is_input Set to 1 if we are looking for '<', 0 for output redirection ('>', '>>', '2>').
void check_and_apply_redirection(char *cmd_str, int is_input)
{
    char *ptr = NULL;
    int type = 0;
    int operator_len = 1;

    if (is_input)
    {
        ptr = strstr(cmd_str, "<");
        type = 3;
    }
    else
    {
        if ((ptr = strstr(cmd_str, "2>")))
        {
            type = 4;
            operator_len = 2;
        }
        else if ((ptr = strstr(cmd_str, ">>")))
        {
            type = 2;
            operator_len = 2;
        }
        else if ((ptr = strstr(cmd_str, ">")))
        {
            type = 1;
            operator_len = 1;
        }
    }

    if (ptr != NULL)
    {
        *ptr = '\0';
        char *filename = strtok(ptr + operator_len, " \t\n");
        if (filename)
        {
            handle_redirection(type, filename);
        }
    }
}

/// @brief Handle zombi processes
void sigchld_handler(int sig)
{
    int status;
    int pid;

    while ((pid = waitpid(-1, &status, WNOHANG)) > 0)
    {
        printf("\nChild %d finished with exit status %d\n", pid, status >> 8);
        fflush(stdout);
    }
}

// Handle Control-C
void sigint_handler(int sig)
{
    printf("\nYou typed Control-C!\n");
    printf("%s", current_prompt);
    fflush(stdout);
}

/// @brief Executes a command pipeline. Splits by '|', manages inter-process pipes,
/// and applies input redirection to the first command and output redirection to the last.
void execute_pipe(char *command)
{
    char *cmds[MAX_COMMANDS];
    int cmd_count = 0;

    char *token = strtok(command, "|");
    while (token != NULL)
    {
        cmds[cmd_count++] = token;
        token = strtok(NULL, "|");
    }

    int pipefd[2];
    int prev_fd = 0;

    for (int i = 0; i < cmd_count; i++)
    {
        if (i < cmd_count - 1)
            pipe(pipefd);

        if (fork() == 0)
        {
            if (prev_fd != 0)
            {
                dup2(prev_fd, STDIN_FILENO);
                close(prev_fd);
            }

            if (i < cmd_count - 1)
            {
                dup2(pipefd[1], STDOUT_FILENO);
                close(pipefd[1]);
                close(pipefd[0]);
            }

            if (i == 0)
                check_and_apply_redirection(cmds[i], 1);
            if (i == cmd_count - 1)
                check_and_apply_redirection(cmds[i], 0);

            char *argv[MAX_COMMANDS];
            int j = 0;
            char *inner = strtok(cmds[i], " ");
            while (inner != NULL)
            {
                argv[j++] = inner;
                inner = strtok(NULL, " ");
            }
            argv[j] = NULL;

            execvp(argv[0], argv);
            perror("execvp failed");
            exit(1);
        }

        if (prev_fd != 0)
            close(prev_fd);
        if (i < cmd_count - 1)
        {
            close(pipefd[1]);
            prev_fd = pipefd[0];
        }
    }

    for (int i = 0; i < cmd_count; i++)
        wait(NULL);
}

int main()
{
    char command[1024];
    char *token;
    char *outfile;
    int i, fd, amper, redirect, retid, status;
    char *argv[10];

    signal(SIGINT, sigint_handler);
    signal(SIGCHLD, sigchld_handler);

    while (1)
    {
        printf("%s", current_prompt);
        fflush(stdout);
        fgets(command, 1024, stdin);
        command[strlen(command) - 1] = '\0'; // replace \n with \0

        if (strchr(command, '|') != NULL)
        {
            execute_pipe(command);
            continue;
        }

        if (command[0] == '\x1B' && command[1] == '[' && command[2] == 'A')
        {
            if (strlen(last_command) > 0)
            {
                memset(command, 0, 1024);
                strcpy(command, last_command);
                printf("%s\n", command);
            }

            else
            {
                printf("No commands in history\n");
                continue;
            }
        }

        if (strcmp(command, "!!") == 0)
        {
            if (strlen(last_command) == 0)
            {
                printf("No commands in history\n");
                continue;
            }
            strcpy(command, last_command);
            printf("%s\n", command);
        }

        else
        {
            strcpy(last_command, command);
        }

        /* parse command line */
        i = 0;
        token = strtok(command, " ");
        while (token != NULL)
        {
            argv[i] = token;
            token = strtok(NULL, " ");
            i++;
        }
        argv[i] = NULL;

        /* Is command empty */
        if (argv[0] == NULL)
            continue;

        if (strcmp(argv[0], "quit") == 0)
        {
            exit(0);
        }

        else if (strcmp(argv[0], "cd") == 0)
        {
            if (chdir(argv[1]) != 0)
            {
                perror("cd failed");
            }
            continue;
        }

        else if (strcmp(argv[0], "status") == 0)
        {
            printf("%d\n", last_status);
            continue;
        }

        else if (strcmp(argv[0], "prompt") == 0)
        {
            if (argv[2] != NULL)
            {
                strcpy(current_prompt, argv[2]);
            }
            continue;
        }

        /* Does command line end with & */
        if (i > 1 && !strcmp(argv[i - 1], "&"))
        {
            amper = 1;
            argv[i - 1] = NULL;
        }
        else
            amper = 0;

        // Redirect standard input overwrite
        if (i > 2 && !strcmp(argv[i - 2], ">"))
        {
            redirect = 1;
            argv[i - 2] = NULL;
            outfile = argv[i - 1];
        }

        // Redirect standard input appending
        else if (i > 2 && !strcmp(argv[i - 2], ">>"))
        {
            redirect = 2;
            argv[i - 2] = NULL;
            outfile = argv[i - 1];
        }

        // Redirect standard output
        else if (i > 2 && !strcmp(argv[i - 2], "<"))
        {
            redirect = 3;
            argv[i - 2] = NULL;
            outfile = argv[i - 1];
        }

        // Redirected standard error
        else if (i > 2 && !strcmp(argv[i - 2], "2>"))
        {
            redirect = 4;
            argv[i - 2] = NULL;
            outfile = argv[i - 1];
        }

        else
        {
            redirect = 0;
        }

        /* for commands not part of the shell command language */

        if (fork() == 0)
        {
            signal(SIGINT, SIG_DFL);

            if (redirect > 0)
            {
                handle_redirection(redirect, outfile);
            }

            execvp(argv[0], argv);
            perror("execvp");
            exit(1);
        }

        /* parent continues here */
        if (amper == 0)
        {
            retid = wait(&status);
            last_status = status >> 8;
        }
    }
}
