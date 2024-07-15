#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/param.h"
#include "user/user.h"

int 
main(int argc, char *argv[]) {
    int i, j;
    char *k;
    char buf[512];
    char *arg[MAXARG];

    for(i = 0; i < argc - 1; i++) {
        arg[i] = argv[i + 1];
    }

    j = 0, k = buf;
    while(read(0, buf + j, 1)) {
        if(buf[j] == '\n') {
            buf[j] = '\0';
            arg[i++] = k;
            k = buf + j + 1;
        }
        j++;
    }
    if (fork() == 0) {
        exec(argv[1], arg);
    }
    wait((int *)0);
    exit(0);
}