#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
// #include "user/ulib.c"

int
main(int argc, char *argv[])
{
    //参数输入错误，打印错误信息
    if(argc <= 1){
    fprintf(2, "usage: sleep times\n");
    exit(1);
    }
    sleep(atoi(argv[1]));
    exit(0);
}
