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
    int p[2];                 // Pipe array for inter-process communication
    int pipe_cmd = 0;         // Pipe command flag
    char *left_command = buf; // For storing the left side of a pipe command
    char *right_command = 0;  // For storing the right side of a pipe command
    char *pointer = buf;

    // Detect if a pipe character ('|') exists in the command
    while (*pointer != '\0') {
        if (*pointer == '|') {
            pipe_cmd = 1;            // Set the pipe flag
            *pointer = '\0';         // Null-terminate the left command
            right_command = pointer + 1; // Start of the right command
            break;
        }
        pointer++;
    }

    // If there's a pipe, set up the pipe handling
    if (pipe_cmd) {
        pipe(p); // Create a pipe: p[0] is read end, p[1] is write end

        // First child process to handle the left command (write to pipe)
        if (fork() == 0) {
            close(1);       // Close stdout
            dup(p[1]);      // Duplicate write end of the pipe to stdout
            close(p[0]);    // Close unused read end of the pipe
            close(p[1]);    // Close the original write end
            run_command(left_command, nbuf, pcp); // Execute the left command
        }

        // Second child process to handle the right command (read from pipe)
        if (fork() == 0) {
            close(0);       // Close stdin
            dup(p[0]);      // Duplicate read end of the pipe to stdin
            close(p[1]);    // Close unused write end of the pipe
            close(p[0]);    // Close the original read end
            run_command(right_command, nbuf, pcp); // Execute the right command
        }

        // Parent process closes both ends of the pipe and waits for children
        close(p[0]); // Close read end in the parent
        close(p[1]); // Close write end in the parent
        wait(0);     // Wait for the left command child to finish
        wait(0);     // Wait for the right command child to finish
        exit(0);     // Exit after handling the pipe
    }

    // Tokenize input for non-pipe commands, checking for redirection symbols
    pointer = buf;
    while (*pointer != '\0' && numargs < 10) {
        while (*pointer == ' ' || *pointer == '\t') {
            pointer++;
        }

        // Input redirection '<'
        if (*pointer == '<') {
            redirection_left = 1;
            pointer++;
            while (*pointer == ' ') pointer++;
            file_name_l = pointer;
            while (*pointer != ' ' && *pointer != '\0') pointer++;
            *pointer++ = '\0';
            continue;
        }

        // Output redirection '>'
        if (*pointer == '>') {
            redirection_right = 1;
            pointer++;
            while (*pointer == ' ') pointer++;
            file_name_r = pointer;
            while (*pointer != ' ' && *pointer != '\0') pointer++;
            *pointer++ = '\0';
            continue;
        }

        // Add arguments if a word is found
        if (ws) {
            arguments[numargs++] = pointer;
            ws = 0;
        }

        // Advance to the next whitespace or end of line to end the current word
        char *spc = strchr(pointer, ' ');
        char *line = strchr(pointer, '\n');

        if (spc && (!line || spc < line)) {
            *spc = '\0';
            pointer = spc + 1;
            ws = 1;
        } else if (line) {
            *line = '\0';
            pointer = line + 1;
            ws = 1;
        } else {
            break;
        }
    }
    arguments[numargs] = 0; // Null-terminate the arguments array

    // Step 1: Check if command is "cd" and execute in parent process
    if (strcmp(arguments[0], "cd") == 0) {
        if (numargs < 2) {
            fprintf(2, "cd: missing argument\n");
            exit(0); // Exit without forking if "cd" failed
        }
        if (chdir(arguments[1]) < 0) {
            fprintf(2, "cd: cannot change directory to %s\n", arguments[1]);
        }
        exit(0); // Exit after handling "cd" in parent
    }

    // Step 2: Handle input redirection if specified
    if (redirection_left) {
        int fd = open(file_name_l, O_RDONLY);
        if (fd < 0) {
            fprintf(2, "Error: cannot open input file %s\n", file_name_l);
            exit(1);
        }
        close(0);
        dup(fd);
        close(fd);
    }

    // Step 3: Handle output redirection if specified
    if (redirection_right) {
        int fd = open(file_name_r, O_WRONLY | O_CREATE | O_TRUNC);
        if (fd < 0) {
            fprintf(2, "Error: cannot open output file %s\n", file_name_r);
            exit(1);
        }
        close(1);
        dup(fd);
        close(fd);
    }

    // Step 4: Fork and execute other commands
    int pid = fork();
    if (pid < 0) {
        fprintf(2, "Error: fork failed\n");
        exit(1);
    } else if (pid == 0) {
        exec(arguments[0], arguments);
        fprintf(2, "Error: command not found: %s\n", arguments[0]);
        exit(1);
    } else {
        wait(0); // Parent waits for child process to complete
    }
    exit(0);
}

int main(void) {
    static char buf[100];

    // Main loop to read and execute commands
    while (getcmd(buf, sizeof(buf)) >= 0) {
        // Check if the command starts with "cd " using manual comparison
        if (buf[0] == 'c' && buf[1] == 'd' && buf[2] == ' ') {
            // Skip "cd " and get the directory path
            char *cd_command = buf + 3; // Pointer to the directory argument
            if (chdir(cd_command) < 0) {
                fprintf(2, "cd: cannot change directory to %s\n", cd_command);
            }
            continue; // Skip the fork if "cd" was handled
        }
        
        // For non-cd commands, fork a child to handle them
        if (fork() == 0) {
            run_command(buf, 100, 0);
        }
        wait(0); // Wait for the command to finish
    }

    exit(0);
}
