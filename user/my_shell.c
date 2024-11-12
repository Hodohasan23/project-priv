#include "kernel/types.h"
#include "user/user.h"
#include "kernel/fcntl.h"

/* Read a line of characters from stdin. */
int getcmd(char *buf, int nbuf) {
    fprintf(1, ">>> ");
    memset(buf, 0, nbuf);

    int r = read(0, buf, nbuf);
    if (r <= 0) return -1; // End of input or error
    buf[r - 1] = '\0'; // Replace newline with null terminator
    return 0;
}

/*
  Recursive function that parses the command in *buf and executes it.
*/
__attribute__((noreturn))
void run_command(char *buf, int nbuf, int *pcp) {
    char *arguments[10];
    int numargs = 0;
    int ws = 1;               // Word start flag
    int redirection_left = 0; // Flag for input redirection
    int redirection_right = 0;// Flag for output redirection
    char *file_name_l = 0;    // File name for input redirection
    char *file_name_r = 0;    // File name for output redirection
    int p[2];                 // For pipes (unused here, reserved for future)
    int pipe_cmd = 0;         // Pipe command flag
    int sequence_cmd = 0;     // Sequence command flag
    char *pointer = buf;

    // Mark unused variables to prevent warnings
    (void)p;
    (void)pipe_cmd;
    (void)sequence_cmd;

    // Tokenize input, checking for redirection symbols and arguments
    while (*pointer != '\0' && numargs < 10) {
        // Skip any leading whitespace
        while (*pointer == ' ' || *pointer == '\t') {
            pointer++;
        }

        // Handle input redirection '<'
        if (*pointer == '<') {
            redirection_left = 1;
            pointer++;
            while (*pointer == ' ') pointer++; // Skip spaces after '<'
            file_name_l = pointer;
            while (*pointer != ' ' && *pointer != '\0') pointer++;
            *pointer++ = '\0';
            continue;
        }
        
        // Handle output redirection '>'
        if (*pointer == '>') {
            redirection_right = 1;
            pointer++;
            while (*pointer == ' ') pointer++; // Skip spaces after '>'
            file_name_r = pointer;
            while (*pointer != ' ' && *pointer != '\0') pointer++;
            *pointer++ = '\0';
            continue;
        }

        // Add argument if it's a word
        if (ws) {
            arguments[numargs++] = pointer; // Start of the word
            ws = 0; // Unset ws flag once processing a word
        }

        // Move to the next whitespace or end of line to end the current word
        char *spc = strchr(pointer, ' '); // Locate the next space
        char *line = strchr(pointer, '\n');
        
        // Determine which delimiter we hit first
        if (spc && (!line || spc < line)) {
            // Null-terminate the current word and move the pointer
            *spc = '\0';
            pointer = spc + 1;
            ws = 1; // Set ws flag for the next word
        } else if (line) {
            // Handle newline as the delimiter, if it’s before the next space
            *line = '\0';
            pointer = line + 1;
            ws = 1; // Set ws flag for the next word
        } else {
            // No more spaces or newlines, break out of the loop
            break;
        }
    }
    arguments[numargs] = 0; // Null-terminate the array of arguments

    // Check for built-in "cd" command
    if (strcmp(arguments[0], "cd") == 0) {
        if (numargs < 2) {
            fprintf(2, "cd: missing argument\n");
            exit(1);
        }
        if (chdir(arguments[1]) < 0) {
            fprintf(2, "cd: cannot change directory to %s\n", arguments[1]);
            exit(1);
        }
        exit(0);
    }

    // If input redirection is specified, set up file descriptor
    if (redirection_left) {
        int fd = open(file_name_l, O_RDONLY);
        if (fd < 0) {
            fprintf(2, "Error: cannot open input file %s\n", file_name_l);
            exit(1);
        }
        close(0);  // Close standard input
        dup(fd);   // Redirect standard input to fd
        close(fd); // Close the original file descriptor
    }

    // If output redirection is specified, set up file descriptor
    if (redirection_right) {
        int fd = open(file_name_r, O_WRONLY | O_CREATE | O_TRUNC);
        if (fd < 0) {
            fprintf(2, "Error: cannot open output file %s\n", file_name_r);
            exit(1);
        }
        close(1);  // Close standard output
        dup(fd);   // Redirect standard output to fd
        close(fd); // Close the original file descriptor
    }

    // Fork a new process to execute the command
    int pid = fork();
    if (pid < 0) {
        fprintf(2, "Error: fork failed\n");
        exit(1);
    } else if (pid == 0) {
        exec(arguments[0], arguments); // Execute the command
        fprintf(2, "Error: command not found: %s\n", arguments[0]);
        exit(1);
    } else {
        wait(0); // Parent waits for child process to complete
    }
    exit(0);
}

int main(void) {
    static char buf[100]; // Buffer to store input command

    // Main loop to read and execute commands
    while (getcmd(buf, sizeof(buf)) >= 0) {
        if (fork() == 0) {
            run_command(buf, 100, 0); // Execute the command in a child process
        }
        wait(0); // Wait for the command to finish
    }

    exit(0); // Exit shell when done
}
