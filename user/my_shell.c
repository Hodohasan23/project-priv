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
    int rd_append = 0; // Created a new flag for appending
    char *file_name_l = 0;    
    char *file_name_r = 0;
    
    int p[2];                 
    int pipe_cmd = 0;         
    char *lPipe = buf; // For storing the left side of a pipe command
    char *rPipe = 0; // For storing the right side of a pipe command
   

    char *pointer = buf;

    // Pipe ('|') check
    while (*pointer != '\0') {
        if (*pointer == '|') {
            pipe_cmd = 1;            
            *pointer = '\0';         // Left side of the pipe is terminated
            rPipe = pointer + 1; // Right side of pipe start
            break;
        }
        pointer++;
    }

    // Pipe handling set-up
    if (pipe_cmd) {
        pipe(p); 

        // Child process to write to pipe
        if (fork() == 0) {
            close(1);       
            dup(p[1]);      
            close(p[0]);    
            close(p[1]);    
            run_command(lPipe, nbuf, pcp); // Left pipe is executed
        }

        // Child process to read from pipe
        if (fork() == 0) {
            close(0);
            dup(p[0]);      
            close(p[1]);    
            close(p[0]);    
            run_command(rPipe, nbuf, pcp); // Right pipe is executed
        }

        // Both ends of the pipes are closed by parent and waits for children
        close(p[0]); 
        close(p[1]); 
        wait(0);     
        wait(0);     
        exit(0);     
    }

    // Tokenize input for non-pipe commands, checking for redirection symbols
    pointer = buf;
    while (*pointer != '\0' && numargs < 10) {
        while (*pointer == ' ' || *pointer == '\t') {
            pointer++;
        }

        // Performs a special check for (>>)
        if (*pointer == '>' && *(pointer + 1) == '>') {
            redirection_right = 1;
            rd_append = 1; // Append set for redirection
            pointer += 2;
            while (*pointer == ' ') pointer++; // After >> spaces are skipped
            file_name_r = pointer;
            while (*pointer != ' ' && *pointer != '\0') pointer++;
            *pointer++ = '\0';
            continue;
        }

        // Output redirection (>) check
        if (*pointer == '>') {
            redirection_right = 1;
            rd_append = 0; // Overwrites for append
            pointer++;
            while (*pointer == ' ') pointer++; // After > spaces are skipped 
            file_name_r = pointer;
            while (*pointer != ' ' && *pointer != '\0') pointer++;
            *pointer++ = '\0';
            continue;
        }

        // Input redirection (<) check
        if (*pointer == '<') {
            redirection_left = 1;
            pointer++;
            while (*pointer == ' ') pointer++; // Skip spaces after <
            file_name_l = pointer;
            while (*pointer != ' ' && *pointer != '\0') pointer++;
            *pointer++ = '\0';
            continue;
        }

        // Checks if its a word to add arguments
        if (ws) {
            arguments[numargs++] = pointer; // Beginning of the word
            ws = 0;
        }

        // Move to the next whitespace or end of line to end the current word
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
    arguments[numargs] = 0; // Arguments array is null terminated

    // If command is "cd" its executed in parent proccess 
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

    // Input redirection handling
    if (redirection_left) {
        int f_descriptor = open(file_name_l, O_RDONLY);
        if (f_descriptor < 0) {
            fprintf(2, "Error: there is an issue opening input file %s\n", file_name_l);
            exit(1);
        }
        close(0);
        dup(f_descriptor);
        close(f_descriptor);
    }

    // Handles append or output redirection 
    if (redirection_right) {
        int f_descriptor = open(file_name_r, O_WRONLY | O_CREATE);
        if (f_descriptor < 0) {
            fprintf(2, "Error: there is an issue opening output file %s\n", file_name_r);
            exit(1);
        }

        if (rd_append) {
            // Append simulated by moving to EOF manually
            char buffer[1];
            while (read(f_descriptor, buffer, 1) > 0) {
                // Ensures that everything is read until EOF
            }
        } else {
            // Overwrite mode: File is truncated
            close(f_descriptor); // File that has just been opened is closed without truncation
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


    // This executes other commands and forks
    int process_id = fork();
    if (process_id < 0) {
        fprintf(2, "Error: Failure with fork\n");
        exit(1);
    } else if (process_id == 0) {
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
    
    int pcp[2];
    pipe(pcp); 

    // Main loop to read and execute commands
    while (getcmd(buf, sizeof(buf)) >= 0) {
        // Using manual comparison check if the command entered starts with 'cd'
        if (buf[0] == 'c' && buf[1] == 'd' && buf[2] == ' ') {
            // 'cd' is skipped and directory path is retrieved
            char *cd_pntr = buf + 3; // Pointer
            if (chdir(cd_pntr) < 0) {
                fprintf(2, "cd error: failed to change directory to %s\n", cd_pntr);
            }
            continue;
        }
        
        // A child is forked to handle non-cd commands
        if (fork() == 0) {
            run_command(buf, 100, pcp);
        }
        wait(0); 
    }

    exit(0);
}
