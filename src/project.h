#ifndef PROJECT_H
#define PROJECT_H

#include "type.h"
#include "util.h"

/* GLOBAL VARIABLES */
extern PROC   proc[NPROC];        // processes
extern MOUNT  mounttab[NMOUNT];   // mount table
extern MINODE m_inodes[NMINODES]; // in memory inodes

extern MINODE *root;
extern PROC   *running;

// global access to command arguments
extern int  myargc;
extern char *myargs[256];

/* BASIC FUNCTIONS */

int ialloc(int dev);
int balloc(int dev);

int idealloc(int dev, int ino);
int bdealloc(int dev, int bno);

int execute(char *cmd);

/* LEVEL 1 FUNCTIONS */

int file_mkdir(char *path);
int file_rmdir(char *path);
int file_creat(char *path);
int file_link(char *src, char *dst);
int file_symlink(char *src, char *dst);
int file_unlink(char *path);

void shell_readlink(char *path);
void shell_ls(char *path);
void shell_cd(char *path);
void shell_pwd();
void shell_stat(char *path);

int file_touch(char *path);
int file_chmod(int mode, char *path);
int file_chown(int own,  char *path);
int file_chgrp(int grp,  char *path);

/* LEVEL 2 FUNCTIONS */

int file_open(char *file, int mode);

int file_close(int fd);

int file_lseek(int fd, int pos);

void shell_pfd();

int file_read(int fd, char buf[], int n);

int file_write(int fd, char buf[], int n);

void shell_cat(char *path);

int file_cp(char *src, char *dst);

int file_mv(char *src, char *dst);

/* LEVEL 3 FUNCTIONS */

void print_mounttab();

void mount(char *filesys, char *path);

void umount(char *filesys);

void pswitch(int uid);

#endif

