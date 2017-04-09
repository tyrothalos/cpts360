#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

#include "project.h"

/* UNEXPORTED FUNCTIONS */

static int alloc_mount()
{
	for (int i = 0; i < NMOUNT; i++)
		if (mounttab[i].dev == 0)
			return i;
	return -1;
}

static int get_mounted(char *filesys)
{
	for (int i = 0; i < NMOUNT; i++)
		if (strcmp(mounttab[i].name, filesys) == 0)
			return i;
	return -1;
}

static int is_mount_busy(int dev)
{
	for (int i = 0; i < NMINODES; i++) {
		if (m_inodes[i].dev == dev && m_inodes[i].ino != 2) {
			printf("inode: %d\n", m_inodes[i].ino);
			return 1;
		}
	}
	return 0;
}

/* EXPORTED FUNCTIONS */

/*
 * print_mounttab:
 *
 * Prints all non-empty entries in the Mount Table to stdout.
 */
void print_mounttab()
{
	for (int i = 0; i < NMOUNT; i++) {
		if (mounttab[i].dev) {
			printf("Name: %s,  ", mounttab[i].name);
			printf("Mount: %s,  ", mounttab[i].mount_name);
			printf("Device: %d\n", mounttab[i].dev);
		}
	}
}

/*
 * mount:
 * @filesys: The ext2 disk image to mount.
 * @path: The path to the mount point.
 *
 * Mounts a disk image to a mount point within the current file system.
 */
void mount(char *filesys, char *path)
{
	int fd, md;
	int dev, ino;
	MINODE *mip = NULL;

	char buf[BLOCK_SIZE];
	SUPER *s = (SUPER *)buf;

	if (path[0] != '/') {
		printf("mount: failed: may only mount relative to root\n");
	} else if (get_mounted(filesys) >= 0) {
		printf("mount: failed: '%s' is already mounted\n", filesys);
	} else if (fd = open(filesys, O_RDWR), fd < 0) {
		printf("mount: failed: could not open '%s'\n", filesys);
	} else if (get_block(fd, SUPER_BLOCK, buf), s->s_magic != SUPER_MAGIC) {
		printf("mount: failed: filesystem is not EXT2\n");
	} else if (file_mkdir(path) < 0) {
		printf("mount: failed: could not create mount point\n");
	} else if (ino = getino(&dev, path), ino == 0) {
		printf("mount: failed: mount point does not exist\n");
	} else if (mip = iget(dev, ino), mip == NULL) {
		printf("mount: error: inode not found\n");
	} else if (md = alloc_mount(), md < 0) {
		printf("mount: failed: mount table is full\n");
	} else {
		char tmp[BLOCK_SIZE];
		get_block(fd, GROUP_BLOCK, tmp);
		GROUP *gd = (GROUP *)tmp;

		MOUNT *m = &mounttab[md];
		m->mounted_inode = mip;
		m->dev = fd;
		m->nblocks = s->s_blocks_count;
		m->ninodes = s->s_inodes_count;
		m->bmap = gd->bg_block_bitmap;
		m->imap = gd->bg_inode_bitmap;
		m->iblk = gd->bg_inode_table;
		strncpy(m->name, filesys, 64);
		strncpy(m->mount_name, path, 64);

		mip->mounted = 1;
		mip->mountptr = m;

		return; // exit here so the mount stays open
	}

	if (mip)
		iput(mip);
	if (fd >= 0)
		close(fd);
}

/*
 * umount:
 * @filesys: The ext2 disk image to unmount.
 *
 * Unmounts a currently mounted disk image from the current file system.
 */
void umount(char *filesys)
{
	int md;
	if (md = get_mounted(filesys), md < 0) {
		printf("umount: failed: '%s' is not mounted\n", filesys);
	} else if (is_mount_busy(mounttab[md].dev)) {
		printf("umount: failed: mount is busy\n");
	} else {
		MOUNT *m = &mounttab[md];

		iput(m->mounted_inode);
		close(m->dev);

		m->mounted_inode = 0;
		m->dev = 0;
		m->nblocks = 0;
		m->ninodes = 0;
		m->bmap = 0;
		m->imap = 0;
		m->iblk = 0;
		memset(m->name, 0, 64);
		memset(m->mount_name, 0, 64);
	}
}

/*
 * pswitch:
 * @uid: The UID of the process to switch to.
 *
 * Switches the running process to the given one.
 */
void pswitch(int uid)
{
	if (uid >= NPROC) {
		printf("switch: failed: invalid uid\n");
	} else {
		running = &proc[uid];
	}
}

