#include "kernel/types.h"    // For basic types like int, char, etc.
#include "user/user.h"       // For system calls like fork, pipe, write, read, close, exit, and printf

int main() {
    int pipe1[2];  // Pipe for parent to child communication
    int pipe2[2];  // Pipe for child to parent communication
    char buffer[5];

    // Create both pipes
    if (pipe(pipe1) < 0 || pipe(pipe2) < 0) {
        printf("Error creating pipes\n");
        exit(1);
    }

    if (fork() == 0) {  // Child process
        // Close unused ends of pipes
        close(pipe1[1]);  // Close write end of pipe1 (parent to child)
        close(pipe2[0]);  // Close read end of pipe2 (child to parent)

        // Read "ping" from the parent
        if (read(pipe1[0], buffer, 4) != 4) {
            printf("Child: Error reading from pipe1\n");
            close(pipe1[0]);
            close(pipe2[1]);
            exit(1);
        }
        buffer[4] = '\0';  // Null-terminate the string
        printf("%d: Received %s, P\n", getpid(), buffer);

        // Send "pong" to the parent
        if (write(pipe2[1], "pong", 4) != 4) {
            printf("Child: Error writing to pipe2\n");
            close(pipe1[0]);
            close(pipe2[1]);
            exit(1);
        }

        // Close the remaining ends in the child process
        close(pipe1[0]);
        close(pipe2[1]);
        exit(0);

    } else {  // Parent process
        // Close unused ends of pipes
        close(pipe1[0]);  // Close read end of pipe1 (parent to child)
        close(pipe2[1]);  // Close write end of pipe2 (child to parent)

        // Send "ping" to the child
        if (write(pipe1[1], "ping", 4) != 4) {
            printf("Parent: Error writing to pipe1\n");
            close(pipe1[1]);
            close(pipe2[0]);
            exit(1);
        }

        // Read "pong" from the child
        if (read(pipe2[0], buffer, 4) != 4) {
            printf("Parent: Error reading from pipe2\n");
            close(pipe1[1]);
            close(pipe2[0]);
            exit(1);
        }
        buffer[4] = '\0';  // Null-terminate the string
        printf("%d: Received %s, R\n", getpid(), buffer);

        // Close the remaining ends in the parent process
        close(pipe1[1]);
        close(pipe2[0]);

        // Wait for the child to finish
        wait(0);
        exit(0);
    }
}
