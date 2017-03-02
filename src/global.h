#ifndef PROJECT_GLOBAL_H
#define PROJECT_GLOBAL_H

#include "type.h"

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

#endif

