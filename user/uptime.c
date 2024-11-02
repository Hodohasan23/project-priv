#include "kernel/types.h"
#include "user/user.h"
int main(int argc, char*argv[]) {
    int ticks = uptime();
    printf("The uptime in ticks is %d\n", ticks);
    exit(0);
}
