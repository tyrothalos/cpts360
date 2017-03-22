#include "project.h"

/* GLOBAL VARIABLE DECLARATIONS */
PROC   proc[NPROC];
MOUNT  mounttab[NMOUNT];
MINODE m_inodes[NMINODES];

MINODE *root;
PROC   *running;
