#include "kernel/types.h"      // For data types used in xv6
#include "kernel/stat.h"       // For file status system calls
#include "user/user.h"       // For user-level system calls and utility functions

int main(int argc, char *argv[]) {
    // Check if an argument was provided
    if (argc != 2) {
        printf(2, "Usage: sleep <number of ticks>\n");
        exit();
    }

    // Convert the argument to an integer
    int ticks = atoi(argv[1]);
    if (ticks <= 0) {
        printf(2, "Invalid number of ticks. Must be a positive integer.\n");
        exit();
    }

    // Call the sleep system call with the given number of ticks
    sleep(ticks);

    // Exit the program
    exit();
}
