#include "kernel/fcntl.h"
#include "kernel/types.h"
#include "user/user.h"

int isSeparator(char c) {
    const char* separators = " -\r\t\n./,";
    return strchr(separators, c) != 0;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        fprintf(2, "Usage: sixfive [filename ...]\n");
        exit(1);
    }

    char buf[32];
    int n = 0;
    char c;

    // 遍历所有文件
    for (int i = 1; i < argc; i++) {
        int fd = open(argv[i], O_RDONLY);
        if (fd < 0) {
            fprintf(2, "sixfive: cannot open %s\n", argv[i]);
            exit(1);
        }

        // 逐字符读取文件
        while (read(fd, &c, 1) == 1) {
            if ('0' <= c && c <= '9') {
                // 是数字，累积到缓冲区
                buf[n++] = c;
            } else if (isSeparator(c)) {
                // 遇到分隔符，结算当前累积的数字
                if (n > 0) {
                    buf[n] = '\0';
                    int num = atoi(buf);
                    if (num % 5 == 0 || num % 6 == 0) printf("%d\n", num);
                    n = 0;
                }
            }
            // 既不是数字也不是分隔符的字符，也当作分隔符处理
            else {
                if (n > 0) {
                    buf[n] = '\0';
                    int num = atoi(buf);    
                    if (num % 5 == 0 || num % 6 == 0) {
                        printf("%d\n", num);
                    }
                    n = 0;
                }
            }
        }

        close(fd);
    }

    // 文件末尾是隐式分隔符，处理剩余的数字
    if (n > 0) {
        buf[n] = '\0';
        int num = atoi(buf);
        if (num % 5 == 0 || num % 6 == 0) printf("%d\n", num);
    }

    exit(0);
}
