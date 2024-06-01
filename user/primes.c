#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int main(int argc, char *argv[])
{
    int end = 35;

    int fdNext[2];
    pipe(fdNext);
    for (int i = 2; i <= end; i++)
    {
        write(fdNext[1], &i, sizeof(i));
    }

    int pid = fork();

    /*第一个线程操作*/
    if (pid != 0)
    {
        // 第一个线程关闭读取管道
        close(fdNext[0]);
        // 第一个线程关闭写入管道
        close(fdNext[1]);
       // wait(&pid);
        // exit(0);
    }

    /*子线程操作*/
    if (pid == 0)
    {
        int thispid = -1;
        while (1)
        {
            int fdLast[2] = {0};
            fdLast[0] = dup(fdNext[0]);
            fdLast[1] = dup(fdNext[1]);
            close(fdNext[0]);
            close(fdNext[1]);
            close(fdLast[1]);

            pipe(fdNext);
            int thisPrime = 0;
            read(fdLast[0], &thisPrime, sizeof(thisPrime));
            printf("prime %d\n", thisPrime);
     

            int thisDeal;
            
            int ifFork = 0;
            while (read(fdLast[0], &thisDeal, sizeof(thisDeal)) > 0)
            {
                if ((thisDeal % thisPrime) != 0)
                {
                    write(fdNext[1], &thisDeal, sizeof(thisDeal));
                    if (ifFork == 0)
                    {
                        ifFork = 1;
                        thispid = fork();
                    }
                    if (thispid == 0)
                    {
                        // 子线程中关闭拷贝过来的父线程的fdLast,子线程只需要父线程的fdNext作为自己的fdLast
                        close(fdLast[0]);
                        // printf("线程：%d结束，处理的素数为： %d\n",getpid(),thisPrime);
                        break;
                    }
                }
            }

            if (thispid != 0 )
            {
                // 父线程中关闭自己的fdLast
                close(fdLast[0]);
                close(fdNext[0]);
                close(fdNext[1]);
                break;
            }
            if(ifFork == 0){
                 // 父线程中关闭自己的fdLast
                close(fdLast[0]);
                close(fdNext[0]);
                close(fdNext[1]);
                break;
            }
        }
         if (thispid != 0 ){
            wait(&thispid);
         }

        exit(0);
    }
    wait(&pid);
    exit(0);
}