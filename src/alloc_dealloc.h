#ifndef PROJECT_ALLOC_DEALLOC_H
#define PROJECT_ALLOC_DEALLOC_H

int ialloc(int dev);
int balloc(int dev);

int idealloc(int dev, int ino);
int bdealloc(int dev, int bno);

#endif

