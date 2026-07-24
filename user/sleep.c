#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/fcntl.h"
#include "user/user.h"

int main(int argc, char* argv[]) {
  if (argc != 2) {
    fprintf(2, "Usage: sleep time...\n");
    exit(1);
  }
  int ret = pause(atoi(argv[1]));
  exit(ret);
}