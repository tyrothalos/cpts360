#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

#include "global.h"
#include "level1.h"
#include "level3.h"
#include "type.h"
#include "util.h"

static int alloc_mount()
{
	int i;
	for (i = 0; i < NMOUNT; i++) {
		if (mounttab[i].dev == 0) {
			return i;
		}
	}
	return -1;
}

static int get_mounted(char *filesys)
{
	int i;
	for (i = 0; i < NMOUNT; i++) {
		if (strcmp(mounttab[i].name, filesys) == 0) {
			return i;
		}
	}
	return -1;
}

void my_mount()
{
	int fd = -1, tab = -1;
	int dev, ino;
	MINODE *mip = NULL;
	
	char buf[BLOCK_SIZE];
	SUPER *s = (SUPER *)buf;

	if (myargc == 1) {
		int i;
		for (i = 0; i < NMOUNT; i++) {
			if (mounttab[i].dev) {
				printf("Name: %s,  ", mounttab[i].name);
				printf("Mount: %s,  ", mounttab[i].mount_name);
				printf("Device: %d\n", mounttab[i].dev);
			}
		}
	} else if (myargc < 3) {
		printf("mount: missing operand\n");
	} else if (myargs[2][0] != '/') {
		printf("mount: failed: may only mount relative to root\n");
	} else if (get_mounted(myargs[1]) >= 0) {
		printf("mount: failed: '%s' is already mounted\n", myargs[1]);
	} else if ((fd = open(myargs[1], O_RDWR)) < 0) {
		printf("mount: failed: could not open '%s'\n", myargs[1]);
	} else if (get_block(fd, SUPER_BLOCK, (char *)s), s->s_magic != SUPER_MAGIC) {
		printf("mount: failed: filesystem is not EXT2\n");
	} else if (file_mkdir(myargs[2]) < 0) {
		printf("mount: failed: could not create mount point\n");
	} else if ((ino = getino(&dev, myargs[2])) == 0) {
		printf("mount: failed: mount point does not exist\n");
	} else if ((mip = iget(dev, ino)) == 0) {
		printf("mount: error: inode not found\n");
	} else if ((tab = alloc_mount()) < 0) {
		printf("mount: failed: mount table is full\n");
	} else {
		char tmp[BLOCK_SIZE];
		get_block(fd, GD_BLOCK, tmp);
		GD *gd = (GD *)tmp;

		MOUNT *m = &mounttab[tab];
		m->mounted_inode = mip; 
		m->dev = fd;
		m->nblocks = s->s_blocks_count;
		m->ninodes = s->s_inodes_count;
		m->bmap = gd->bg_block_bitmap;
		m->imap = gd->bg_inode_bitmap;
		m->iblk = gd->bg_inode_table;
		strcpy(m->name, myargs[1]);
		strcpy(m->mount_name, myargs[2]);

		mip->mounted = 1;
		mip->mountptr = m;

		return;
	}

	if (mip)
		iput(mip);
	if (fd >= 0)
		close(fd);
}

static int is_mount_busy(int dev)
{
	int i;
	for (i = 0; i < NMINODES; i++) {
		if (m_inodes[i].dev == dev && m_inodes[i].ino != 2) {
			printf("inode: %d\n", m_inodes[i].ino);
			return 1;
		}
	}
	return 0;
}

void my_umount()
{
	int tab = -1;
	if (myargc == 1) {
		printf("umount: missing operand\n");
	} else if ((tab = get_mounted(myargs[1])) < 0) {
		printf("umount: failed: '%s' is not mounted\n", myargs[1]);
	} else if (is_mount_busy(mounttab[tab].dev)) {
		printf("umount: failed: mount is busy\n");
	} else {
		MOUNT *m = &mounttab[tab];
		m->name[0] = 0;
		m->mount_name[0] = 0;
		m->mounted_inode->mounted = 0; 

		close(m->dev);
		m->dev = 0;
		
		iput(m->mounted_inode);
		m->mounted_inode = 0;

		m->nblocks = 0;
		m->ninodes = 0;
		m->bmap = 0;
		m->imap = 0;
		m->iblk = 0;
	}
}

void my_switch()
{
	int uid;
	if (myargc == 1) {
		printf("switch: missing operand\n");	
	} else if (sscanf(myargs[1], "%u", &uid) < 1) {
		printf("switch: invalid input\n");
	} else if (uid >= NPROC) {
		printf("switch: failed: invalid uid\n");
	} else {
		running = &proc[uid];
	}
}

