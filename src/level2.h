#ifndef PROJECT_LEVEL2_H
#define PROJECT_LEVEL2_H

int file_open(char *file, int mode);
int file_close(int fd);
int file_lseek(int fd, int pos);
int file_read(int fd, char buf[], int n);
int file_write(int fd, char buf[], int n);
int file_cp(char *src, char *dst);
int file_mv(char *src, char *dst);

#endif

