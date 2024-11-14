#include "kernel/types.h"
#include "user/user.h"
#include "kernel/fcntl.h"

#define MAX 10

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
    char *arguments[MAX];
    int numargs = 0;


    int redirection_left = 0;
    int redirection_right = 0;
    char *file_name_l = 0;
    char *file_name_r = 0;

    int pipe_cmd = 0;
    int sequence_cmd = 0;
    char *lPipe = buf;
    char *rPipe = 0;
    char *nc = 0;

    char *pointer = buf;

    // Check for `;` or `|`
    for (int i = 0; i < nbuf && *pointer != '\0'; i++) {
        if (*pointer == '|') {
            pipe_cmd = 1;
            *pointer = '\0'; // Null-terminate left side of the pipe
            rPipe = pointer + 1; // Start of right side of pipe
            break;
        } else if (*pointer == ';') {
            sequence_cmd = 1;
            *pointer = '\0'; // Current command terminated
            nc = pointer + 1; // Next command start
            break;
        }
        pointer++;
    }

    // Handle piping
    if (pipe_cmd) {
        int p[2];
        pipe(p);

        // First child process for the left command
        if (fork() == 0) {
            close(1);
            dup(p[1]);
            close(p[0]);
            close(p[1]);
            run_command(lPipe, nbuf, pcp);
        }

        // Second child process for the right command
        if (fork() == 0) {
            close(0);
            dup(p[0]);
            close(p[1]);
            close(p[0]);
            run_command(rPipe, nbuf, pcp);
        }

        close(p[0]);
        close(p[1]);
        wait(0);
        wait(0);
        exit(0);
    }

    // Handle sequential commands
    if (sequence_cmd) {
        if (fork() == 0) {
            run_command(buf, nbuf, pcp);
        }
        wait(0);
        run_command(nc, nbuf, pcp);
        exit(0);
    }

    // Tokenize the input for the command and arguments
    pointer = buf;
    while (*pointer != '\0' && numargs < MAX - 1) {
        while (*pointer == ' ' || *pointer == '\t') pointer++;
        arguments[numargs++] = pointer;
        while (*pointer != ' ' && *pointer != '\0') pointer++;
        if (*pointer != '\0') *pointer++ = '\0';
    }
    arguments[numargs] = 0;

    // Handle redirection operators and remove them from arguments
    for (int i = 0; i < numargs; i++) {
        // Output redirection ">"
        if (strcmp(arguments[i], ">") == 0 && arguments[i + 1] != 0) {
            redirection_right = 1;
            file_name_r = arguments[i + 1];
            for (int j = i; arguments[j] != 0; j++) {
                arguments[j] = arguments[j + 2];
            }
            i--;
            numargs -= 2;
        }
        // Input redirection "<"
        else if (strcmp(arguments[i], "<") == 0 && arguments[i + 1] != 0) {
            redirection_left = 1;
            file_name_l = arguments[i + 1];
            for (int j = i; arguments[j] != 0; j++) {
                arguments[j] = arguments[j + 2];
            }
            i--;
            numargs -= 2;
        }
    }

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
    if (redirection_left && file_name_l) {
        int fd_in = open(file_name_l, O_RDONLY);
        if (fd_in < 0) {
            fprintf(2, "Error: cannot open input file %s\n", file_name_l);
            exit(1);
        }
        close(0);
        dup(fd_in);
        close(fd_in);
    }

    // Handle output redirection
    if (redirection_right && file_name_r) {
        int fd_out = open(file_name_r, O_WRONLY | O_CREATE | O_TRUNC);
        if (fd_out < 0) {
            fprintf(2, "Error: cannot open output file %s\n", file_name_r);
            exit(1);
        }
        close(1);
        dup(fd_out);
        close(fd_out);
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
