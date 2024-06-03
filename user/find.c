#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"

void compare(char *FindFileName)
{
}
void RecursiveFind(char *FindDir, char *FindFileName)
{

    char buf[512], *p;
    int fd;
    struct stat st;
    struct dirent de;

    if ((fd = open(FindDir, 0)) < 0)
    {
        fprintf(2, "find: cannot open %s\n", FindDir);
        return;
    }

    if (fstat(fd, &st) < 0)
    {
        fprintf(2, "find: cannot stat %s\n", FindDir);
        close(fd);
        return;
    }
    if (st.type == T_FILE)
    {
        // compare(FindFileName);
        fprintf(2, "%s\n", FindDir);
        return;
    }
    if (st.type != T_DIR)
    {
        return;
    }

    if (strlen(FindDir) + 1 + DIRSIZ + 1 > sizeof buf)
    {
        fprintf(2, "find: path too long\n");
        return;
    }

    strcpy(buf, FindDir);
    p = buf + strlen(buf);
    *p++ = '/';
    while (read(fd, &de, sizeof(de)) == sizeof(de))
    {
        if (de.inum == 0)
            continue;
        if (de.inum == 0 || strcmp(de.name, ".") == 0 || strcmp(de.name, "..") == 0)
        {
            continue;
        }
        memmove(p, de.name, DIRSIZ);
        p[DIRSIZ] = 0;
        if (stat(buf, &st) < 0)
        {
            fprintf(2, "find: cannot stat %s\n", buf);
            continue;
        }
        if (st.type == T_DIR && strcmp(p, ".") != 0 && strcmp(p, "..") != 0)
        {
            RecursiveFind(buf, FindFileName);
        }
        else if (strcmp(FindFileName, p) == 0)
            printf("%s\n", buf);
    }
}
int main(int argc, char *args[])
{
    if (argc != 3)
    {
        fprintf(2, "Usage: find <directory> <filename>\n");
        exit(0);
    }
    else
    {
        RecursiveFind(args[1], args[2]);
    }
    exit(0);
    // int fd;
    // char* FindDir = args[1];
    // if ((fd = open(FindDir, 0)) < 0)
    // {
    //     fprintf(2, "find: cannot open %s\n", FindDir);
    // }
    //  else fprintf(2, "find: open %s\n", FindDir);
    //  exit(0);
}
