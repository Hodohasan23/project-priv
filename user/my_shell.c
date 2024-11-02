#include "kernel/types.h"
#include "user/user.h"
#include "kernel/fcntl.h"

/* Read a line of characters from stdin. */
int getcmd(char *buf, int nbuf) {
    // Print a prompt for the user to enter a command
    printf(">>> ");
    
    // Clear the buffer to prepare for new input
    memset(buf, 0, nbuf);
    
    // Read input from stdin (keyboard) into `buf`
    if (read(0, buf, nbuf) <= 0) { // 0 is the file descriptor for stdin
        return -1; // Return -1 if there's no input (end-of-file)
    }
    
    return 0; // Return 0 if reading input was successful
}

/*
  This function will execute the command parsed from `buf`.
  - `buf` contains the command as a string.
*/
void run_command(char *buf) {
    char *arguments[10]; // Array to store command arguments
    int numargs = 0;
    
    // Step 1: Tokenize the input manually
    char *ptr = buf;
    while (*ptr != '\0' && numargs < 10) {
        // Skip any leading whitespace
        while (*ptr == ' ' || *ptr == '\n') {
            *ptr = '\0'; // Replace whitespace with null terminator
            ptr++;
        }
        
        // If there's a word, set it as an argument
        if (*ptr != '\0') {
            arguments[numargs++] = ptr; // Start of a word
        }
        
        // Move to the end of the word
        while (*ptr != ' ' && *ptr != '\n' && *ptr != '\0') {
            ptr++;
        }
    }
    arguments[numargs] = 0; // Null-terminate the array of arguments

    // Step 2: Check if the command is "cd" (change directory)
    if (strcmp(arguments[0], "cd") == 0) {
        // `cd` needs at least one argument (the directory to change to)
        if (numargs < 2) {
            printf("cd: missing argument\n");
            return;
        }
        // Change directory using `chdir`
        if (chdir(arguments[1]) < 0) {
            printf("cd: cannot change directory to %s\n", arguments[1]);
        }
        return; // Return to the main loop to prompt for the next command
    }

    // Step 3: Create a child process to run the command
    int pid = fork();
    if (pid < 0) {
        printf("Error: fork failed\n");
        return;
    }
    if (pid == 0) {
        // Child process: execute the command
        exec(arguments[0], arguments); // Replace process with the command
        // If exec fails, print an error message
        printf("Error: command not found: %s\n", arguments[0]);
        exit(1); // Exit with an error code if exec fails
    } else {
        // Parent process: wait for the child process to complete
        wait(0);
    }
}

/* Main function */
int main(void) {
    static char buf[100]; // Buffer to store the input command

    // Loop to continually read commands and execute them
    while (getcmd(buf, sizeof(buf)) >= 0) {
        run_command(buf); // Run the command the user entered
    }
    exit(0); // Exit the shell if there’s no input
}
