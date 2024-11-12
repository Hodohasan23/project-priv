#include "kernel/types.h"
#include "user/user.h"
#include "kernel/fcntl.h"

/* Read a line of characters from stdin. */
int getcmd(char *buf, int nbuf) {
    // Print a prompt for the user to enter a command
    fprintf(1, ">>>");

    // Clear the buffer to prepare for new input
    memset(buf, 0, nbuf);

    int r = read(0, buf, nbuf);
    if (r <= 0)
        return -1;

    buf[r] = '\0';
    return 0; // Return 0 if reading input was successful
}

/*
  This function will execute the command parsed from `buf`.
  - `buf` contains the command as a string.
*/

void run_command(char *buf) {
    char *arguments[10]; // Array to store command arguments
    int numargs = 0;
    char *pointer = buf;

    // Redirection flags and filenames
    int redirection_left = 0;   // Flag for input redirection
    int redirection_right = 0;  // Flag for output redirection
    char *file_name_l = 0;      // File name for input redirection
    char *file_name_r = 0;      // File name for output redirection

    // Tokenize the input manually using your tokenization approach
    while (*pointer != '\0' && numargs < 10) {
        // Step 1: Skip any leading whitespace
        while (*pointer == ' ' || *pointer == '\n') {
            pointer++;
        }

        // Detect redirection symbols, but avoid modifying tokenization
        if (*pointer == '>') {
            // Output redirection detected
            redirection_right = 1;
            pointer++;
            while (*pointer == ' ') pointer++;    // Skip spaces
            file_name_r = pointer;                // Set output filename
            while (*pointer != ' ' && *pointer != '\n' && *pointer != '\0') pointer++;
            *pointer = '\0';                      // Null-terminate filename
        } else if (*pointer == '<') {
            // Input redirection detected
            redirection_left = 1;
            pointer++;
            while (*pointer == ' ') pointer++;    // Skip spaces
            file_name_l = pointer;                // Set input filename
            while (*pointer != ' ' && *pointer != '\n' && *pointer != '\0') pointer++;
            *pointer = '\0';                      // Null-terminate filename
        } else {
            // Step 2: If a word is found, add it as an argument
            if (*pointer != '\0') {
                arguments[numargs++] = pointer; // Store start of the word
            }

            // Step 3: Find the next space or end of line
            char *spc = strchr(pointer, ' '); // Use strchr to locate the next space
            char *line = strchr(pointer, '\n');
            
            // Determine which delimiter we hit first
            if (spc && (!line || spc < line)) {
                // Null-terminate the current word and move the pointer
                *spc = '\0';
                pointer = spc + 1;
            } else if (line) {
                // Handle newline as the delimiter, if it's before the next space
                *line = '\0';
                pointer = line + 1;
            } else {
                // No more spaces or newlines, break out of the loop
                break;
            }
        }
    }
    arguments[numargs] = 0; // Null-terminate the array of arguments

    // Handle case where no command was entered (just spaces or newlines)
    if (numargs == 0) {
       return; // Return to prompt for the next command
    }

    // Step 4: Check for built-in "exit" command
    if (strcmp(arguments[0], "exit") == 0) {  // <-- Added
        exit(0); // Terminate the shell process                     // <-- Added
    }                                                              // <-- Added

    // Step 5: Check for built-in "cd" command
    if (strcmp(arguments[0], "cd") == 0) {
        if (numargs < 2) {
            printf("cd: missing argument\n");
            return;
        }
        if (chdir(arguments[1]) < 0) {
            printf("cd: cannot change directory to %s\n", arguments[1]);
        }
        return; // Return to prompt for the next command
    }

    // Step 6: Create a child process to run external command
    int pid = fork();
    if (pid < 0) {
        printf("Error: fork failed\n");
        return;
    }
    if (pid == 0) {
        // Child process: handle redirection

 // Input redirection
        if (redirection_left && file_name_l) {
            int fd = open(file_name_l, O_RDONLY);
            if (fd < 0) {
                printf("Error: cannot open file %s for input\n", file_name_l);
                exit(1);
            }
            close(0);       // Close stdin
            if (dup(fd) < 0) {
                printf("Error: dup failed for input redirection\n");
                exit(1);
            }
            close(fd);      // Close the original file descriptor
        }

       // Output redirection
        if (redirection_right && file_name_r) {
            int fd = open(file_name_r, O_WRONLY | O_CREATE | O_TRUNC);
            if (fd < 0) {
                printf("Error: cannot open file %s for output\n", file_name_r);
                exit(1);
            }
            close(1);       // Close stdout
            if (dup(fd) < 0) {
                printf("Error: dup failed for output redirection\n");
                exit(1);
            }
            close(fd);      // Close the original file descriptor
        }

        // Execute the command
        exec(arguments[0], arguments);
        printf("Error: command not found: %s\n", arguments[0]);
        exit(1);
    } else {
        // Parent process: wait for the child to complete
        wait(0);
    }
}



int main(void) {
    static char buf[100]; // Buffer to store the input command

    // Main shell loop
    while (getcmd(buf, sizeof(buf)) >= 0) { // Keep reading commands
        // Call run_command to execute the command entered by the user
        run_command(buf);
    }

    // Exit when done (like when an EOF is received)
    exit(0);
}
