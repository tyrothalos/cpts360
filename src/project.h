#ifndef PROJECT_H
#define PROJECT_H

#include "type.h"
#include "util.h"

/* GLOBAL VARIABLES */
extern OFT    oft[NOFT];          // opened file instance
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

/* LEVEL 2 FUNCTIONS */

int file_open(char *file, int mode);

int file_close(int fd);

int file_lseek(int fd, int pos);

int file_read(int fd, char buf[], int n);

int file_write(int fd, char buf[], int n);

int file_cp(char *src, char *dst);

int file_mv(char *src, char *dst);

/* LEVEL 3 FUNCTIONS */

void print_mounttab();

void mount(char *filesys, char *path);

void umount(char *filesys);

#endif

