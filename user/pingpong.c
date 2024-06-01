#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
// #include "user/ulib.c"

int
main(int argc, char *argv[])
{
    int fd[2];
    pipe(fd);
    int retpid = fork();
    if(retpid == 0){
        char* chr = "a";
        read(fd[0],chr,sizeof(chr));
        printf("%d: received ping\n",getpid());
        write(fd[1],chr,sizeof(chr));
 
    }
    else{
        char* chr = "b";
        write(fd[1],chr,sizeof(chr));
        wait(&retpid);
        printf("%d: received pong\n",getpid());
        read(fd[0],chr,sizeof(chr));
      
    }
    exit(0);

}
