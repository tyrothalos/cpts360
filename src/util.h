#ifndef PROJECT_UTIL_H
#define PROJECT_UTIL_H

#include "type.h"

void get_block(int dev, int blk, char buf[]);
void put_block(int dev, int blk, char buf[]);

SUPER get_super_block(int dev);
void put_super_block(int dev, SUPER super);

GD get_group_block(int dev);
void put_group_block(int dev, GD group);

int tokenize(char *path, char *delim, char *buf[]);
int search(MINODE *iptr, char *name);
int has_perm(MINODE *mip, unsigned int perm);
void clear_blocks(MINODE *mip);

int getino(int *dev, char *name);
MINODE *iget(int dev, int ino);
void iput(MINODE *mip);

#endif

