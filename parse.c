#include "cscshell.h"
#include <ctype.h>
#include <stdbool.h>

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

/**
 * Searches for the "PATH" variable within a linked list of environment variables.
 *
 * @param variables A pointer to the head of a linked list of Variable structures.
 *                  Each Variable structure represents an environment variable
 *                  with a name and value.
 * @return A pointer to the Variable structure representing the "PATH" environment
 *         variable if found, NULL otherwise.
 */
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

/**
 * Finds the first command in a line.
 * 
 * @param line The command line string.
 * @return Dynamically allocated string of the first command or NULL on failure.
 */
char *find_first_command(char *line) {
    char *start = line;
    char *end = start;

    // Find the end of the first command
    while (*end != ' ' && *end != '<' && *end != '>' && *end != '\0' && *end != '|') {
        end++;
    }

    // Extract the first command
    char *first_command = malloc((end - start + 1) * sizeof(char));
    strncpy(first_command, start, end - start);
    first_command[end - start] = '\0';

    return first_command;
}

/**
 * Initializes command with executable path and handles redirection.
 * This function was created with help from Github Copilot.
 * The following link contains prompt #1 that was used to create this function:
 * https://docs.google.com/document/d/1Z8r5L1gA5MtiVTdLJAWj3TX0uQaEWlSS9jbzDqdxf20/edit?usp=sharing
 * 
 * @param cmd Command to be initialized.
 * @param variables Environment variables for path resolution.
 * @param line "Command line" input.
 * @return True if successfully initialized, otherwise False.
 */
bool handle_redirection_command_initialization(Command *cmd, Variable **variables, char **line) {
    // assign first parameter as executable path
    char *first_command = cmd->args[0];
    cmd->exec_path = resolve_executable(first_command, find_path_variable(*variables)); 

    int found = 0; // flag to indicate if < or > is found

    for (int i = 0; line[i] != NULL; i++) {
        if (!strcmp(line[i], ">") || !strcmp(line[i], ">>") || !strcmp(line[i], "<")) {
            found = 1; // set flag to 1 if < or > is found
            break;
        }
    }

    if (!found) { // no redirection found
        cmd->redir_in_path = NULL;
        cmd->redir_out_path = NULL;
        cmd->redir_append = 0;
    }

    for (int index = 0; cmd->args[index]; index++) {
        // Output redirection
        if (!strcmp(cmd->args[index], ">")) { // If no file after operand, return error
            if (cmd->args[index + 1] == NULL) {
                ERR_PRINT(ERR_EXECUTE_LINE);
                return false;
            }
            else {
                cmd->redir_out_path = strdup(cmd->args[index + 1]);
                cmd->redir_append = 0;
                cmd->args[index] = NULL; 
            }
        }
        // Append operation
        else if (!strcmp(cmd->args[index], ">>")) { // If no file after operand, return error
            if (cmd->args[index + 1] == NULL) {
                ERR_PRINT(ERR_EXECUTE_LINE);
                return false;
            }
            else {
                cmd->redir_out_path = strdup(cmd->args[index + 1]);
                cmd->redir_append = 1;
                cmd->args[index] = NULL;
            }
        }
        // Input redirection
        else if (!strcmp(cmd->args[index], "<")) { // If no file after operand, return error
            if (cmd->args[index + 1] == NULL) {
                ERR_PRINT(ERR_EXECUTE_LINE);
                return false;
            }
            else { 
                cmd->redir_in_path = strdup(cmd->args[index + 1]);
                cmd->redir_append = 0;
                cmd->args[index] = NULL;
            }
        }
    }
    cmd->stdin_fd = STDIN_FILENO;
    cmd->stdout_fd = STDOUT_FILENO;
    return true;    
}

/**
 * Creates a linked list of Command structures from an array of command strings.
 * Each command is parsed for arguments and environment variables are replaced. 
 * Redirections within each command are handled during initialization.
 * The linked list iteration (lines 225 - 239) was created with help from Github Copilot.
 * The following link contains prompt #2 that was used to create this function:
 * https://docs.google.com/document/d/1Z8r5L1gA5MtiVTdLJAWj3TX0uQaEWlSS9jbzDqdxf20/edit?usp=sharing
 *
 * @param line Array of command strings, split by pipes.
 * @param variables Pointer to the head of the list of environment variables.
 * @return Pointer to the head of the linked list of Command structures.
 */
Command *set_linked_list(char **line, Variable **variables) {
    Command *head = NULL, *curr = NULL;
    // Assuming 'commands_split' is an array of command strings ending with a NULL marker
    for (int i = 0; line[i] != NULL; i++) {
        Command *new_cmd = malloc(sizeof(Command));
        if (new_cmd == NULL) {
            exit(EXIT_FAILURE); // Handle allocation failure
        }
        memset(new_cmd, 0, sizeof(Command)); // Initialize command structure

        if (head == NULL) {
            head = new_cmd; // First command becomes the head
        } else {
            curr->next = new_cmd; // Link new command to the end of the list
        }
        curr = new_cmd; // Move 'curr' to the last command in the list

        char **list_split_by_spaces = malloc(sizeof(char*) * MAX_SINGLE_LINE);
        if (list_split_by_spaces == NULL) {
            exit(EXIT_FAILURE);
        }
        
        char *replaced = replace_variables_mk_line(line[i], *variables);
        memset(list_split_by_spaces, 0, sizeof(char*) * MAX_SINGLE_LINE);
        
        int j = 0;
        char *savePtr;
        char *token = strtok_r(replaced, " ", &savePtr);
        
        while (token != NULL) {
            list_split_by_spaces[j++] = token; // Assign the token to the array and increment number of tokens
            token = strtok_r(NULL, " ", &savePtr); // Continue splitting line
        }
        
        list_split_by_spaces[j] = NULL; // Null-terminate the array of tokens

        curr->args = list_split_by_spaces;

        // process for redirection
        bool initialized = handle_redirection_command_initialization(curr, variables, line);
        if (!initialized) {
            return (Command *) -1;
        }
        
    }

    return head;
}

/**
 * Parses a command line, splits it by pipes, and creates a linked list of Command structures.
 * Handles the parsing of the command line, replacing environment variables, and managing redirections.
 * Lines 298 - 310 were generated from pseudocode provided in Piazza post @2549
 * 
 * @param start The command line string to be parsed.
 * @param num_args The number of arguments in the command line. Currently unused.
 * @param variables Pointer to the head of the environment variables list.
 * @return Head of the linked list of Command structures representing the parsed command line.
 */
Command *parse_command(char *start, int num_args, Variable **variables) {
    if (find_path_variable(*variables) == NULL) {
        ERR_PRINT(ERR_EXECUTE_LINE);
        return (Command *) -1;
    }

    // split the command line into tokens and accounting for pipes
    char **list_of_split_commands = malloc(sizeof(char*) * MAX_SINGLE_LINE);
    if (list_of_split_commands == NULL) {
        exit(EXIT_FAILURE);
    }
   
    memset(list_of_split_commands, 0, sizeof(char*) * MAX_SINGLE_LINE);
    int idx = 0;
    char *savePtr;
    char *command = strtok_r(start, "|", &savePtr);
    
    while (command != NULL) {
        list_of_split_commands[idx++] = command; // Assign the command to the array and increment number of commands
        command = strtok_r(NULL, "|", &savePtr); // Continue splitting commandLine
    }
    list_of_split_commands[idx] = NULL; // Null-terminate the array of commands

    Command *head = set_linked_list(list_of_split_commands, variables);
    
    return head;
}

/**
 * Counts the number of arguments in a command line.
 * 
 * This function takes into consideration spaces as separators between arguments and ignores spaces 
 * adjacent to redirection operators ('<' and '>'). It returns the total number of arguments found.
 * 
 * @param line The command line string to count arguments from.
 * @return The number of arguments in the command line.
 */
int count_args(char *line) {
    int num_args = 1;
    char *ptr = line;
    while (*ptr) {
        if (isspace((unsigned char)*ptr)) {
            // Check if the next character is not a redirection operator
            if (*(ptr + 1) != '<' && *(ptr + 1) != '>') {
                num_args++;
            }
            while (isspace((unsigned char)*ptr)) ptr++;
        } else {
            ptr++;
        }
    }
    return num_args;
}

Command *parse_line(char *line, Variable **variables) {
    if (line == NULL || *line == '\0' || *line == '#') {
        return NULL; // Handle empty lines or comments immediately
    }

    while(isspace((unsigned char)*line)) line++;

    char *trimmed_line = strdup(line);
    if(trimmed_line == NULL) {
        exit(EXIT_FAILURE);
    }

    if (*trimmed_line == '#') {
        return NULL; // Handle comments
    }

    if (*trimmed_line == '\0') {
        return NULL; // Handle empty lines
    }

    char *equalsPtr = strchr(trimmed_line, '=');

    // variable assignment
    if (equalsPtr) {
        // If '=' is the first non-whitespace character, it's an error
        if (equalsPtr == trimmed_line) {
            ERR_PRINT(ERR_VAR_START);
            return (Command *) -1;
        }

        *equalsPtr = '\0'; // Split name and value
        char *name = trimmed_line;
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

        // Add new variable
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
        if (strchr(trimmed_line, '$')) {
            char *new_line = replace_variables_mk_line(trimmed_line, *variables);
            if (new_line == NULL) {
                return (Command *) -1;
            } else if (new_line == (char *) -1) {
                return (Command *) -1;
            } 
            int num_args = count_args(new_line);
            Command *cmd = parse_command(new_line, num_args, variables);
            free(new_line);
            return cmd;
        }

        // If no variable replacement is needed, we can proceed with parsing the command
        char *ptr = trimmed_line;
        int num_args = count_args(ptr);
        // This could be the place where you handle commands or other types of input
        // For now, we just print a placeholder message

        Command *cmd = parse_command(ptr, num_args, variables);
        return cmd; // This will return a single command
    }

    free(trimmed_line);
    return NULL;
}

/**
 * Searches for a variable by name in a linked list of environment variables.
 *
 * @param variables The head of the linked list of environment variables.
 * @param name The name of the variable to search for.
 * @return A pointer to the found Variable structure, or NULL if not found.
 */
Variable *find_variable(Variable *variables, const char *name) {
    for (Variable *current = variables; current; current = current->next) {
        if (strcmp(current->name, name) == 0) {
            return current;
        }
    }
    return NULL;
}

/**
 * Extracts a variable name from a given position in a string.
 *
 * This function handles both braced (${VAR}) and non-braced ($VAR) variable names.
 * It copies the extracted variable name into a provided buffer.
 *
 * @param ptr A pointer to the character in the string immediately before the variable name.
 * @param var_name A buffer where the extracted variable name will be stored.
 * @return A pointer to the character immediately following the variable name in the string.
 */
const char *extract_var_name(const char *ptr, char *var_name) {
    int is_braced = *(ptr + 1) == '{';
    const char *var_start;
    if (is_braced) {
        var_start = ptr + 2; // Skip past the '$' and '{'
    } else {
        var_start = ptr + 1; // Skip past the '$'
    }
    const char *var_end = var_start;

    while (*var_end && (isalnum((unsigned char)*var_end) || *var_end == '_')) var_end++;
    if (is_braced && *var_end != '}') return NULL;

    size_t var_name_len = var_end - var_start;
    strncpy(var_name, var_start, var_name_len);
    var_name[var_name_len] = '\0';

    if (is_braced) {
        return var_end + 1;
    } else {
        return var_end;
    }
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

    // Iterate through the line and replace variables with their values
    while (*ptr) {
        // Check if the current character is a variable
        if (*ptr == '$') {
            char var_name[MAX_SINGLE_LINE];
            ptr = extract_var_name(ptr, var_name);
            if (ptr == NULL) {
                ERR_PRINT(ERR_EXECUTE_LINE);
                free(new_line);
                return NULL;
            }

            // Find the variable in the list and replace it with its value
            Variable *var = find_variable(variables, var_name);
            // If the variable is not found, return an error
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
            // Copy the character to the new line
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
        free(var->name);
        free(var->value);
        free(var);
    }
}
