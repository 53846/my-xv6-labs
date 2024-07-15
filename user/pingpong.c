#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
  int p1[2], p2[2];
  char buffer[1];

  pipe(p1);
  pipe(p2);
  if (fork() == 0) {
    close(p1[1]);
    close(p2[0]);
    
    read(p1[0], buffer, 1);
    close(p1[0]);
    printf("%d: received ping\n", getpid());
    write(p2[1], buffer, 1);
    close(p2[1]);
  } else {
    close(p1[0]);
    close(p2[1]);

    write(p1[1], "a", 1);
    close(p1[1]);
    read(p2[0], buffer, 1);
    // printf("%s\n", buffer);
    close(p2[0]);
    printf("%d: received pong\n", getpid());
  }
  exit(0);
}
