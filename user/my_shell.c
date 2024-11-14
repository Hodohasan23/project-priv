#include "kernel/types.h"
#include "user/user.h"
#include "kernel/fcntl.h"

/* Read a line of characters from stdin. */
int getcmd(char *buf, int nbuf) {
    fprintf(1, ">>> ");
    memset(buf, 0, nbuf);

    int r = read(0, buf, nbuf);
    if (r <= 0) return -1; // Error or end of input
    buf[r - 1] = '\0'; // Null terminator replaces new line
    return 0;
}

/*
  Recursive function which parses the command in *buf and executes it.
*/
__attribute__((noreturn))
void run_command(char *buf, int nbuf, int *pcp) {
    char *arguments[10];
    int numargs = 0;

    /* Word start/end */
    int ws = 1;

    int redirection_left = 0;
    int redirection_right = 0;
    int rd_append = 0; // Flag for appending
    char *file_name_l = 0;
    char *file_name_r = 0;

    int p[2];
    int pipe_cmd = 0;
    int sequence_cmd = 0; // Flag for sequential commands
    char *lPipe = buf;    // Left side of a pipe command
    char *rPipe = 0;      // Right side of a pipe command
    char *next_command = 0; // Next command after `;`

    char *pointer = buf;

    // Check for `;` or `|`
    for (int i = 0; i < nbuf && *pointer != '\0'; i++) {
        if (*pointer == '|') {
            pipe_cmd = 1;
            *pointer = '\0';       // Null-terminate left side of the pipe
            rPipe = pointer + 1;   // Start of right side of pipe
            break;
        } else if (*pointer == ';') {
            sequence_cmd = 1;
            *pointer = '\0';       // Null-terminate the current command
            next_command = pointer + 1; // Start of the next command
            break;
        }
        pointer++;
    }

    // Handle piping
    if (pipe_cmd) {
        pipe(p);

        // First child process to write to pipe
        if (fork() == 0) {
            close(1);       // Redirect stdout to pipe
            dup(p[1]);
            close(p[0]);    // Close unused read end
            close(p[1]);    // Close original write end
            run_command(lPipe, nbuf, pcp); // Execute left command
        }

        // Second child process to read from pipe
        if (fork() == 0) {
            close(0);       // Redirect stdin to pipe
            dup(p[0]);
            close(p[1]);    // Close unused write end
            close(p[0]);    // Close original read end
            run_command(rPipe, nbuf, pcp); // Execute right command
        }

        // Parent closes both ends and waits for children
        close(p[0]);
        close(p[1]);
        wait(0);
        wait(0);
        exit(0);
    }

    // Handle sequential commands
    if (sequence_cmd) {
        if (fork() == 0) {
            // Execute the current command
            run_command(buf, nbuf, pcp);
        }
        wait(0); // Wait for the current command to finish
        run_command(next_command, nbuf, pcp); // Recursively execute next command
        exit(0); // Exit after executing the sequence
    }

    // Tokenize input for non-pipe commands, checking for redirection symbols
    pointer = buf;
    while (*pointer != '\0' && numargs < 10) {
        while (*pointer == ' ' || *pointer == '\t') {
            pointer++;
        }

        // Output redirection (>>)
        if (*pointer == '>' && *(pointer + 1) == '>') {
            redirection_right = 1;
            rd_append = 1;
            pointer += 2;
            while (*pointer == ' ') pointer++;
            file_name_r = pointer;
            while (*pointer != ' ' && *pointer != '\0') pointer++;
            *pointer++ = '\0';
            continue;
        }

        // Output redirection (>)
        if (*pointer == '>') {
            redirection_right = 1;
            rd_append = 0;
            pointer++;
            while (*pointer == ' ') pointer++;
            file_name_r = pointer;
            while (*pointer != ' ' && *pointer != '\0') pointer++;
            *pointer++ = '\0';
            continue;
        }

        // Input redirection (<)
        if (*pointer == '<') {
            redirection_left = 1;
            pointer++;
            while (*pointer == ' ') pointer++;
            file_name_l = pointer;
            while (*pointer != ' ' && *pointer != '\0') pointer++;
            *pointer++ = '\0';
            continue;
        }

        // Add argument if word is found
        if (ws) {
            arguments[numargs++] = pointer;
            ws = 0;
        }

        // Find end of word
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
    arguments[numargs] = 0;

    // Handle `cd` command in parent process
    if (strcmp(arguments[0], "cd") == 0) {
        if (numargs < 2) {
            fprintf(2, "cd: Path argument is required\n");
            exit(0);
        }
        if (chdir(arguments[1]) < 0) {
            fprintf(2, "cd error: failed to change directory to %s\n", arguments[1]);
        }
        exit(0);
    }

    // Handle input redirection
    if (redirection_left) {
        int f_descriptor = open(file_name_l, O_RDONLY);
        if (f_descriptor < 0) {
            fprintf(2, "Error: cannot open input file %s\n", file_name_l);
            exit(1);
        }
        close(0);
        dup(f_descriptor);
        close(f_descriptor);
    }

    // Handle output redirection
    if (redirection_right) {
        int f_descriptor = open(file_name_r, O_WRONLY | O_CREATE);
        if (f_descriptor < 0) {
            fprintf(2, "Error: cannot open output file %s\n", file_name_r);
            exit(1);
        }

        if (rd_append) {
            // Simulate append by moving to end of file
            char buffer[1];
            while (read(f_descriptor, buffer, 1) > 0);
        } else {
            close(f_descriptor);
            f_descriptor = open(file_name_r, O_WRONLY | O_CREATE | O_TRUNC);
            if (f_descriptor < 0) {
                fprintf(2, "Error: cannot open output file %s\n", file_name_r);
                exit(1);
            }
        }

        close(1);
        dup(f_descriptor);
        close(f_descriptor);
    }

    // Fork and execute other commands
    int process_id = fork();
    if (process_id < 0) {
        fprintf(2, "Error: Failure with fork\n");
        exit(1);
    } else if (process_id == 0) {
        exec(arguments[0], arguments);
        fprintf(2, "Error: command not found: %s\n", arguments[0]);
        exit(1);
    } else {
        wait(0);
    }
    exit(0);
}

int main(void) {
    static char buf[100];
    
    int pcp[2];
    pipe(pcp);

    // Main loop to read and execute commands
    while (getcmd(buf, sizeof(buf)) >= 0) {
        if (buf[0] == 'c' && buf[1] == 'd' && buf[2] == ' ') {
            char *cd_pntr = buf + 3;
            if (chdir(cd_pntr) < 0) {
                fprintf(2, "cd error: failed to change directory to %s\n", cd_pntr);
            }
            continue;
        }
        
        // Fork a child for non-cd commands
        if (fork() == 0) {
            run_command(buf, 100, pcp);
        }
        wait(0);
    }

    exit(0);
}
