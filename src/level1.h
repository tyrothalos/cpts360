#ifndef PROJECT_LEVEL1_H
#define PROJECT_LEVEL1_H

int file_mkdir(char *path);
int file_rmdir(char *path);
int file_creat(char *path);
int file_link(char *src, char *dst);
int file_unlink(char *path);

void my_ls(); // char *path
void my_cd(); // char *path
void my_pwd();

void my_mkdir(); // char *path
void my_creat(); // char *path
void my_rmdir(); // char *path
void my_rm();

void my_link();     // char *src, char *dst
void my_symlink();  // char *src, char *dst
void my_readlink(); // char *path
void my_unlink();   // char *path

void my_stat();  // char *path
void my_touch(); // char *path
void my_chmod(); // char *mod, char *path
void my_chown(); // char *own, char *path
void my_chgrp(); // char *grp, char *path

#endif

