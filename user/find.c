#include "kernel/types.h"
#include "kernel/fcntl.h" // O_RDONLY
#include "kernel/fs.h"    // struct dirent, DIRSIZ
#include "kernel/stat.h"  // struct stat, T_DIR, T_FILE
#include "kernel/param.h"
#include "user/user.h" // open, read, close, fstat, printf...

#define NULL 0

void
check_path(const char *path, const char *name, char **cmd);

int
main(int argc, char *argv[])
{
  if(argc < 3) {
    fprintf(2, "Usage: find [path] [name] [-exec command ...]\n");
    exit(1);
  }

  const char *path = argv[1];
  const char *name = argv[2];
  char **cmd = NULL;

  if(argc > 3) {
    if(strcmp(argv[3], "-exec") != 0) {
      fprintf(2, "The third argument should be -exec.\n");
      exit(1);
    }

    if(argc == 4) {
      fprintf(2, "No command was provided after -exec.\n");
      exit(1);
    }

    cmd = argv + 4;
  }
  check_path(path, name, cmd);
  return 0;
}

void
check_path(const char *path, const char *name, char **cmd)
{
  int fd = open(path, O_RDONLY);
  struct stat st;
  struct dirent de;
  if(fd < 0) {
    fprintf(2, "Can not open the path.\n");
    return;
  }
  fstat(fd, &st);
  if(st.type == T_DIR) {
    while(read(fd, &de, sizeof(de)) == sizeof(de)) {
      if(!strcmp(de.name, ".") || !strcmp(de.name, "..") || !de.inum) {
        continue;
      }
      char buf[512];
      strcpy(buf, path);
      char *p = buf + strlen(buf); // p 指向 buf 末尾的 '\0'
      *p++ = '/';
      memmove(p, de.name, DIRSIZ);
      p[DIRSIZ] = 0;
      check_path(buf, name, cmd);
    }
  } else {
    // 当前 path 即为完整路径，判断最后文件名是否符合即可
    const char *p;
    for(p = path + strlen(path); *p != '/' && p > path; p--)
      ;
    p++;
    if(!strcmp(p, name)) {
      // printf("%s\n", path);
      if(!cmd) {
        printf("%s\n", path);
      } else {
        int rc = fork();
        if(rc < 0) {
          fprintf(2, "fork failed.\n ");
          exit(1);
        } else if(rc == 0) {
          char *new_cmd[MAXARG];
          int i;
          for(i = 0; cmd[i]; i++) {
            new_cmd[i] = cmd[i];
          }
          new_cmd[i] = (char*)path;
          new_cmd[i+1] = 0;
          exec(new_cmd[0], new_cmd);
          fprintf(2, "exec failed.\n");
          exit(1);
        } else {
          wait(NULL);
        }
      }
    }
  }
  close(fd);
}