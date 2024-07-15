#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

void 
set_up_new_pipeline(int *fd_prev) {
    close(fd_prev[1]);
    int p = 0;
    if (!read(fd_prev[0], &p, 4)) {
        return;
    }
    printf("prime %d\n", p);

    int num = 0;
    int fd_next[2];
    pipe(fd_next);
    if (fork() == 0) {
        set_up_new_pipeline(fd_next);
    } else {
        close(fd_next[0]);
        while (read(fd_prev[0], &num, 4)) {
            if (num % p == 0) continue;
            write(fd_next[1], &num, 4);    
        }
        close(fd_next[1]);
        wait((int *) 0);
    }
}

int
main(int argc, char *argv[])
{
    int i;
    int fd[2];

    pipe(fd);
    for (i = 2; i <= 35; i++) {
        write(fd[1], &i, 4);
    }
    set_up_new_pipeline(fd);
    exit(0);
}
