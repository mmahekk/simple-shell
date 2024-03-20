/*****************************************************************************/
/*                           CSC209-24s A3 CSCSHELL                          */
/*       Copyright 2024 -- Demetres Kostas PhD (aka Darlene Heliokinde)      */
/*****************************************************************************/

#include "cscshell.h"
#include <ctype.h>

#define CONTINUE_SEARCH NULL


// COMPLETE
char *resolve_executable(const char *command_name, Variable *path){

    if (command_name == NULL || path == NULL){
        return NULL;
    }

    if (strcmp(command_name, CD) == 0){
        return strdup(CD);
    }

    if (strcmp(path->name, PATH_VAR_NAME) != 0){
        ERR_PRINT(ERR_NOT_PATH);
        return NULL;
    }

    char *exec_path = NULL;

    if (strchr(command_name, '/')){
        exec_path = strdup(command_name);
        if (exec_path == NULL){
            perror("resolve_executable");
            return NULL;
        }
        return exec_path;
    }

    // we create a duplicate so that we can mess it up with strtok
    char *path_to_toke = strdup(path->value);
    if (path_to_toke == NULL){
        perror("resolve_executable");
        return NULL;
    }
    char *current_path = strtok(path_to_toke, ":");

    do {
        DIR *dir = opendir(current_path);
        if (dir == NULL){
            ERR_PRINT(ERR_BAD_PATH, current_path);
            closedir(dir);
            continue;
        }

        struct dirent *possible_file;

        while (exec_path == NULL) {
            // rare case where we should do this -- see: man readdir
            errno = 0;
            possible_file = readdir(dir);
            if (possible_file == NULL) {
                if (errno > 0){
                    perror("resolve_executable");
                    closedir(dir);
                    goto res_ex_cleanup;
                }
                // end of files, break
                break;
            }

            if (strcmp(possible_file->d_name, command_name) == 0){
                // +1 null term, +1 possible missing '/'
                size_t buflen = strlen(current_path) +
                    strlen(command_name) + 1 + 1;
                exec_path = (char *) malloc(buflen);
                // also sets remaining buf to 0
                strncpy(exec_path, current_path, buflen);
                if (current_path[strlen(current_path)-1] != '/'){
                    strncat(exec_path, "/", 2);
                }
                strncat(exec_path, command_name, strlen(command_name)+1);
            }
        }
        closedir(dir);

        // if this isn't null, stop checking paths
        if (possible_file) break;

    } while ((current_path = strtok(CONTINUE_SEARCH, ":")));

res_ex_cleanup:
    free(path_to_toke);
    return exec_path;
}

Variable *find_path_variable(Variable *variables) {
    Variable *current = variables;
    while (current != NULL) {
        if (strcmp(current->name, "PATH") == 0) {
            return current;
        }
        current = current->next;
    }
    return NULL; // PATH variable not found
}

Command *parse_command(char *start, int num_args, Variable **variables) {
    Command *cmd = malloc(sizeof(Command));
    if (cmd == NULL) {
        exit(EXIT_FAILURE);
    }
    memset(cmd, 0, sizeof(Command)); // Initialize all fields to 0/NULL

    // Allocate space for arguments (+1 for NULL terminator)
    cmd->args = malloc(sizeof(char*) * (num_args + 1));
    if (cmd->args == NULL) {
        exit(EXIT_FAILURE);
    }

    char *saveptr; // For use with strtok_r
    char *token = strtok_r(start, " ", &saveptr);
    if (find_path_variable(*variables) == NULL) {
        ERR_PRINT(ERR_EXECUTE_LINE);
        return (Command *) -1;
    }
    
    cmd->exec_path = resolve_executable(token, find_path_variable(*variables));
    cmd->args[0] = strdup(token); // First arg is the command itself

    // Parse remaining arguments
    int i = 1;
    while ((token = strtok_r(NULL, " ", &saveptr)) != NULL) {
        cmd->args[i++] = strdup(token);
    }
    cmd->args[i] = NULL; // NULL terminate the array
    cmd->next = NULL; // This is the last command in the list
    cmd->stdin_fd = STDIN_FILENO;
    cmd->stdout_fd = STDOUT_FILENO;
    cmd->redir_in_path = NULL;
    cmd->redir_out_path = NULL;
    cmd->redir_append = 0;

    return cmd;
}

Command *parse_line(char *line, Variable **variables) {
    if (line == NULL || *line == '\0' || *line == '#') {
        return NULL; // Handle empty lines or comments immediately
    }

    char *start = line;
    while (isspace((unsigned char)*start)) start++; // Skip leading whitespace

    if (*start == '#') {
        return NULL; // Handle comments
    }

    if (*start == '\0') {
        return NULL; // Handle empty lines
    }

    char *equalsPtr = strchr(start, '=');

    if (equalsPtr) {
        // If '=' is the first non-whitespace character, it's an error
        if (equalsPtr == start) {
            ERR_PRINT(ERR_VAR_START);
            return (Command *) -1;
        }

        *equalsPtr = '\0'; // Split name and value
        char *name = start;
        char *value = equalsPtr + 1; // Include leading spaces in the value

        // Validate variable name
        for (char *ptr = name; *ptr; ptr++) {
            if (!isalpha((unsigned char)*ptr) && *ptr != '_') {
                ERR_PRINT(ERR_VAR_NAME, name);
                return (Command *) -1;
            }
        }

        // Update or add variable
        for (Variable *current = *variables; current; current = current->next) {
            if (strcmp(current->name, name) == 0) {
                free(current->value);
                current->value = strdup(value);
                return NULL;
            }
        }

        Variable *new_var = (Variable *)malloc(sizeof(Variable));
        if (!new_var) {
            exit(EXIT_FAILURE);
        }
        new_var->name = strdup(name);
        new_var->value = strdup(value);
        new_var->next = *variables;
        *variables = new_var;
        return NULL;
        }

    // Handle cases where the line is not a variable assignment
    if (!equalsPtr) {
        // first we need to check if we need to do variable replacement
        // this is done by checking if the line contains a '$'
        if (strchr(start, '$')) {
            char *new_line = replace_variables_mk_line(start, *variables);
            if (new_line == NULL) {
                return (Command *) -1;
            } else if (new_line == (char *) -1) {
                return (Command *) -1;
            } 
            int num_args = 1;
            char *ptr = new_line;
            while (*ptr) {
                if (isspace((unsigned char)*ptr)) {
                    num_args++;
                    while (isspace((unsigned char)*ptr)) ptr++;
                } else {
                    ptr++;
                }
            }
            Command *cmd = parse_command(new_line, num_args, variables);
            free(new_line);
            return cmd;
        }

        int num_args = 1;
        char *ptr = start;
        while (*ptr) {
            if (isspace((unsigned char)*ptr)) {
                num_args++;
                while (isspace((unsigned char)*ptr)) ptr++;
            } else {
                ptr++;
            }
        }
        // This could be the place where you handle commands or other types of input
        // For now, we just print a placeholder message

        Command *cmd = parse_command(start, num_args, variables);
        return cmd; // This will return a single command
    }

    return NULL;
}

Variable *find_variable(Variable *variables, const char *name) {
    for (Variable *current = variables; current; current = current->next) {
        if (strcmp(current->name, name) == 0) {
            return current;
        }
    }
    return NULL;
}

const char *extract_var_name(const char *ptr, char *var_name) {
    int is_braced = *(ptr + 1) == '{';
    const char *var_start = ptr + (is_braced ? 2 : 1);
    const char *var_end = var_start;

    while (*var_end && (isalnum((unsigned char)*var_end) || *var_end == '_')) var_end++;
    if (is_braced && *var_end != '}') return NULL;

    size_t var_name_len = var_end - var_start;
    strncpy(var_name, var_start, var_name_len);
    var_name[var_name_len] = '\0';

    return var_end + (is_braced ? 1 : 0);
}

/*
** This function is partially implemented for you, but you may
** scrap the implementation as long as it produces the same result.
**
** Creates a new line on the heap with all named variable *usages*
** replaced with their associated values.
**
** Returns NULL if replacement parsing had an error, or (char *) -1 if
** system calls fail and the shell needs to exit.
*/
char *replace_variables_mk_line(const char *line, Variable *variables) {
    size_t new_line_length = strlen(line) + 1;
    char *new_line = (char *)malloc(new_line_length);
    if (new_line == NULL) {
        return (char *) -1;
    }

    const char *ptr = line;
    int new_idx = 0;

    while (*ptr) {
        if (*ptr == '$') {
            char var_name[MAX_SINGLE_LINE];
            ptr = extract_var_name(ptr, var_name);
            if (ptr == NULL) {
                ERR_PRINT(ERR_EXECUTE_LINE);
                free(new_line);
                return NULL;
            }

            Variable *var = find_variable(variables, var_name);
            if (var) {
                size_t var_value_len = strlen(var->value);
                new_line = realloc(new_line, new_line_length - strlen(var_name) + var_value_len);
                strcpy(new_line + new_idx, var->value);
                new_idx += var_value_len;
            } else {
                ERR_PRINT(ERR_EXECUTE_LINE);
                free(new_line);
                return NULL;
            }
        } else {
            new_line[new_idx++] = *ptr++;
        }
    }

    new_line[new_idx] = '\0';
    return new_line;
}

void free_variable(Variable *var, uint8_t recursive) {
    // Check if the variable is NULL
    if (var == NULL) return;

    // If recursive is true, free the entire list of variables
    if (recursive) {
        while (var != NULL) {
            Variable *nextVar = var->next; // Save next variable
            free(var->name); // Free the name of the current variable
            free(var->value); // Free the value of the current variable
            free(var); // Free the current variable
            var = nextVar; // Move to next variable
        }
    } else {
        // Only free the specified variable (non-recursive case)
        free(var->name);
        free(var->value);
        free(var);
    }
}
