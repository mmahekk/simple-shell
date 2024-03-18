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


int *execute_line(Command *head){
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

    return NULL;
}


/*
** Forks a new process and execs the command
** making sure all file descriptors are set up correctly.
**
** Parent process returns -1 on error.
** Any child processes should not return.
*/
int run_command(Command *command){
    pid_t pid; // added for now
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
    pid = getpid();
}


int run_script(char *file_path, Variable **root) {
    // Open the script file for reading
    // FILE *file = fopen(file_path, "r");
    // if (!file) {
    //     perror("fopen");
    //     return -1; // Error opening file
    // }

    // char line[MAX_SINGLE_LINE];
    // int line_number = 0;
    // int ret_code = 0; // Assume success, unless an error occurs

    // // Read the script file line by line
    // while (fgets(line, sizeof(line), file)) {
    //     line_number++;
    //     line[strcspn(line, "\n")] = '\0'; // Remove the newline character at the end of the line

    //     // Parse the line into commands
    //     Command *commands = parse_line(line, root);
    //     if (commands == (Command *) -1){
    //         fprintf(stderr, "Error parsing line %d: %s\n", line_number, line);
    //         ret_code = -1; // Indicate error
    //         continue; // Skip to the next line
    //     }
    //     if (commands == NULL) continue; // Skip lines that do not contain commands

    //     // Execute the parsed commands
    //     int *last_ret_code_pt = execute_line(commands);
    //     if (last_ret_code_pt == (int *) -1){
    //         fprintf(stderr, "Error executing line %d: %s\n", line_number, line);
    //         free(last_ret_code_pt); // Make sure to free any allocated memory
    //         ret_code = -1; // Indicate error
    //         break; // Stop execution on error
    //     }
    //     // Clean up after executing the line
    //     free(last_ret_code_pt);
    //     // Also, free the commands parsed for this line
    //     free_command(commands);
    // }

    // fclose(file); // Close the script file
    // return ret_code; // Return the final execution status
}


void free_command(Command *command){
    
}
