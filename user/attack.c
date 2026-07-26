#include "kernel/types.h"
#include "kernel/fcntl.h"
#include "user/user.h"
#include "kernel/riscv.h"

int
main(int argc, char *argv[])
{
  // Your code here.
  char *p = sbrk(8*4096);
  if (p == (char*)-1) {
    exit(1);
  }
  for (int i = 0; i < 8*4096 - 16; i++) {
    if (p[i] =='T') {
      char tmp[15];
      memcpy(tmp, p + i, 15);
      if (!strcmp(tmp, "This may help.")) {
        for (char *c = p + i + 16; *c; c++) {
          printf("%c", *c);
        }
        printf("\n");
      }
    }
  }
  exit(1);
}
