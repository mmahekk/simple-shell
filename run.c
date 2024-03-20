/*****************************************************************************/
/*                           CSC209-24s A3 CSCSHELL                          */
/*       Copyright 2024 -- Demetres Kostas PhD (aka Darlene Heliokinde)      */
/*****************************************************************************/

#include "cscshell.h"
#include <unistd.h>


// COMPLETE
int cd_cscshell(const char *target_dir){
    if (target_dir == NULL) {
        char user_buff[MAX_USER_BUF];
        if (getlogin_r(user_buff, MAX_USER_BUF) != 0) {
           perror("run_command");
           return -1;
        }
        struct passwd *pw_data = getpwnam((char *)user_buff);
        if (pw_data == NULL) {
           perror("run_command");
           return -1;
        }
        target_dir = pw_data->pw_dir;
    }

    if(chdir(target_dir) < 0){
        perror("cd_cscshell");
        return -1;
    }
    return 0;
}

int *execute_line(Command *head) {
    if (head == NULL) {
        return NULL;
    }

    int *exit_status = malloc(sizeof(int));
    if (exit_status == NULL) {
        perror("malloc");
        return NULL;
    }

    Command *cmd = head;
    while (cmd != NULL) {
        if (strcmp(cmd->args[0], "cd") == 0) {
            // Handle cd command
            char *path = cmd->args[1] ? cmd->args[1] : getpwuid(getuid())->pw_dir;
            *exit_status = cd_cscshell(path);
            if (*exit_status != 0) {
                // cd command failed, stop executing the rest of the commands
                break;
            }
        } else {
            pid_t pid = fork();
            if (pid < 0) {
                perror("fork");
                *exit_status = -1;
                return exit_status;
            } else if (pid == 0) {
                // Child process
                if (cmd->redir_in_path != NULL) {
                    int fd = open(cmd->redir_in_path, O_RDONLY);
                    if (fd < 0) {
                        perror("open");
                        exit(EXIT_FAILURE);
                    }
                    dup2(fd, STDIN_FILENO);
                    close(fd);
                }

                execv(cmd->exec_path, cmd->args);
                perror("execv");
                exit(EXIT_FAILURE);
            } else {
                // Parent process
                waitpid(pid, exit_status, 0);
                if (WIFEXITED(*exit_status) && WEXITSTATUS(*exit_status) != 0) {
                    // Command failed, stop executing the rest of the commands
                    break;
                }
            }
        }
        cmd = cmd->next;
    }

    return exit_status;

    #ifdef DEBUG
    printf("\n***********************\n");
    printf("BEGIN: Executing line...\n");
    #endif

    #ifdef DEBUG
    printf("All children created\n");
    #endif

    // Wait for all the children to finish

    #ifdef DEBUG
    printf("All children finished\n");
    #endif

    #ifdef DEBUG
    printf("END: Executing line...\n");
    printf("***********************\n\n");
    #endif
}

/*
** Forks a new process and execs the command
** making sure all file descriptors are set up correctly.
**
** Parent process returns -1 on error.
** Any child processes should not return.
*/
int run_command(Command *command){
    pid_t pid = fork(); // added for now
    #ifdef DEBUG
    printf("Running command: %s\n", command->exec_path);
    printf("Argvs: ");
    if (command->args == NULL){
        printf("NULL\n");
    }
    else if (command->args[0] == NULL){
        printf("Empty\n");
    }
    else {
        for (int i=0; command->args[i] != NULL; i++){
            printf("%d: [%s] ", i+1, command->args[i]);
        }
    }
    printf("\n");
    printf("Redir out: %s\n Redir in: %s\n",
           command->redir_out_path, command->redir_in_path);
    printf("Stdin fd: %d | Stdout fd: %d\n",
           command->stdin_fd, command->stdout_fd);
    #endif


    #ifdef DEBUG
    printf("Parent process created child PID [%d] for %s\n", pid, command->exec_path);
    #endif
    return -1;
    if (pid < 0) {
        perror("fork");
        return -1;
    } else if (pid == 0) {
        // Child process
        if (command->redir_in_path != NULL) {
            int fd = open(command->redir_in_path, O_RDONLY);
            if (fd < 0) {
                perror("open");
                exit(EXIT_FAILURE);
            }
            dup2(fd, STDIN_FILENO);
            close(fd);
        }

        execv(command->exec_path, command->args);
        perror("execv");
        exit(EXIT_FAILURE);
    } else {
        // Parent process
        return pid;
    }
}


int run_script(char *file_path, Variable **root){
    FILE *file = fopen(file_path, "r");
    if (file == NULL){
        fprintf(stderr, ERR_INIT_SCRIPT, file_path);
        return -1;
    }
    char line[MAX_SINGLE_LINE +1];
    fgets(line, MAX_SINGLE_LINE, file);
    return 0;
}


void free_command(Command *command){
    
}
