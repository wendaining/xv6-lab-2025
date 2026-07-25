#include "kernel/types.h"
#include "kernel/fcntl.h" // O_RDONLY
#include "kernel/fs.h"    // struct dirent, DIRSIZ
#include "kernel/stat.h"  // struct stat, T_DIR, T_FILE
#include "user/user.h" // open, read, close, fstat, printf...

void
check_path(const char *path, const char *name);

int
main(int argc, char *argv[])
{
  if(argc != 3) {
    fprintf(2, "Usage: find [path] [name]\n");
    exit(1);
  }
  const char *path = argv[1];
  const char *name = argv[2];
  check_path(path, name);
  return 0;
}

void
check_path(const char *path, const char *name)
{
  int fd = open(path, O_RDONLY);
  struct stat st;
  struct dirent de;
  if(fd < 0) {
    fprintf(2, "Can not open the path.\n");
    return;
  }
  fstat(fd, &st);
  if (st.type == T_DIR) {
    while (read(fd, &de, sizeof(de)) == sizeof(de)) {
      if (!strcmp(de.name, ".") || !strcmp(de.name, "..")) {
        continue;
      }
      char buf[512];
      strcpy(buf, path);
      char *p = buf + strlen(buf); // p 指向 buf 末尾的 '\0'
      *p++ = '/';
      memmove(p, de.name, DIRSIZ);
      p[DIRSIZ] = 0;
      check_path(buf, name);
    }
  } else {
      // 当前 path 即为完整路径，判断最后文件名是否符合即可
      const char *p;
      for (p = path + strlen(path); *p != '/' && p > path; p--)
        ;
      p++;
      if (!strcmp(p, name)) {
        printf("%s\n", path);
      }
  }
  close(fd);
}