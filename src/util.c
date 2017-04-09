#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "project.h"

/* UNEXPORTED FUNCTIONS */

static void inode_get_block(MINODE *mip)
{
	char buf[BLOCK_SIZE];
	GROUP *gd = (GROUP *)buf;

	// get inode's group to find where inode_table is
	get_block(mip->dev, GROUP_BLOCK, buf);

	int ratio = INODE_SIZE / 128;
	int per_block = BLOCK_SIZE / INODE_SIZE;
	int blk = ((mip->ino - 1) / per_block) + gd->bg_inode_table;
	int off = ((mip->ino - 1) % per_block);

	// get block inode is in
	get_block(mip->dev, blk, buf);
	// select target inode
	INODE *inode = (INODE *)buf + off*ratio;
	// copy to minode
	mip->inode = *inode;
}

static void inode_put_block(MINODE *mip)
{
	char buf[BLOCK_SIZE];
	GROUP *gd = (GROUP *)buf;

	// get inode's group to find where inode_table is
	get_block(mip->dev, GROUP_BLOCK, buf);

	int ratio = INODE_SIZE / 128;
	int per_block = BLOCK_SIZE / INODE_SIZE;
	int blk = ((mip->ino - 1) / per_block) + gd->bg_inode_table;
	int off = ((mip->ino - 1) % per_block);

	// get block inode is in
	get_block(mip->dev, blk, buf);
	// select target inode
	INODE *inode = (INODE *)buf + off*ratio;
	// copy minode to inode
	*inode = mip->inode;
	// put back to disk
	put_block(mip->dev, blk, buf);
}

/*
 * clear_direct:
 * @dev: The device number the blocks are on.
 * @blocks: The array of blocks to clear.
 * @n: The size of the array.
 *
 * Clears the blocks in the given array.
 */
static void clear_direct(int dev, unsigned int blocks[], int n)
{
	for (int i = 0; i < n; i++) {
		if (!blocks[i]) {
			break;
		}
		bdealloc(dev, blocks[i]);
		blocks[i] = 0;
	}
}

static void clear_indirect(int dev, unsigned int *addr, int n, int depth)
{
	if (*addr != 0) {
		unsigned int blocks[256];
		get_block(dev, *addr, (char *)blocks);
		if (depth > 0) {
			for (int i = 0; i < n; i++) {
				clear_indirect(dev, &blocks[i], 256, depth-1);
			}
		} else {
			clear_direct(dev, blocks, n);
		}
		put_block(dev, *addr, (char *)blocks);
		bdealloc(dev, *addr);
		*addr = 0;
	}
}

/* EXPORTED FUNCTIONS */

void get_block(int dev, int blk, char buf[])
{
	lseek(dev, (long)blk*BLOCK_SIZE, SEEK_SET);
	read(dev, buf, BLOCK_SIZE);
}

void put_block(int dev, int blk, char buf[])
{
	lseek(dev, (long)blk*BLOCK_SIZE, SEEK_SET);
	write(dev, buf, BLOCK_SIZE);
}

SUPER get_super_block(int dev)
{
	SUPER super;
	lseek(dev, (long)SUPER_BLOCK*BLOCK_SIZE, SEEK_SET);
	read(dev, &super, sizeof(super));
	return super;
}

void put_super_block(int dev, SUPER super)
{
	lseek(dev, (long)SUPER_BLOCK*BLOCK_SIZE, SEEK_SET);
	write(dev, &super, sizeof(super));
}

GROUP get_group_block(int dev)
{
	GROUP group;
	lseek(dev, (long)GROUP_BLOCK*BLOCK_SIZE, SEEK_SET);
	read(dev, &group, sizeof(group));
	return group;
}

void put_group_block(int dev, GROUP group)
{
	lseek(dev, (long)GROUP_BLOCK*BLOCK_SIZE, SEEK_SET);
	write(dev, &group, sizeof(group));
}

void clear_blocks(MINODE *mip)
{
	INODE *ip = &mip->inode;

	// handle direct blocks
	clear_direct(mip->dev, ip->i_block, 12);

	// handle indirect blocks
	clear_indirect(mip->dev, &ip->i_block[12], 256, 0);

	// handle double indirect blocks
	clear_indirect(mip->dev, &ip->i_block[13], 256, 1);

	// finally reset size
	ip->i_size = 0;
	mip->dirty = 1;
}

int tokenize(char *path, char *delim, char *buf[])
{
	int n = 0;
	for (char *tok = strtok(path, delim); tok != NULL;
			tok = strtok(NULL, delim)) {
		buf[n++] = tok;
	}
	return n;
}

/*
 * has_perm:
 * @mip: The inode to check the permissions of.
 * @perm: The permission to check for.
 *
 * Checks if the given inode has the given permission.
 *
 * Returns: 0 if the inode does not have the permission, otherwise nonzero.
 */
int has_perm(MINODE *mip, unsigned int perm)
{
	if (running->uid == 0)
		return 1;

	INODE *ip = &mip->inode;
	unsigned int mode = ip->i_mode;

	if (ip->i_uid == running->uid) {
		mode &= 00700;
		return (((mode >> 6) & perm) == perm);
	} else if (ip->i_gid == running->gid) {
		mode &= 00070;
		return (((mode >> 3) & perm) == perm);
	} else {
		mode &= 00007;
		return ((mode & perm) == perm);
	}
}

/*
 * search:
 * @parent: The parent inode to search.
 * @name: The name of the inode to search for.
 *
 * Searches for an inode with the given name inside of the given parent inode.
 * If it is found, then the inode number of the found node is returned.
 *
 * Returns: The inode number if found, otherwise 0.
 */
int search(MINODE *parent, char *name)
{
	int len = strlen(name);
	for (int i = 0; i < 12; i++) {
		char buf[BLOCK_SIZE];
		DIR *dp = (DIR *)buf;

		get_block(parent->dev, parent->inode.i_block[i], buf);

		for (int j = 0; j < BLOCK_SIZE; j += dp->rec_len) {
			dp = (DIR *)(buf + j);
			if (dp->rec_len <= 0)
				return 0;
			if (len == dp->name_len && strncmp(name, dp->name, len) == 0)
				return dp->inode;
		}
	}
	return 0;
}

/*
 * getino:
 * @dev: The device the inode is on.
 * @name: The name of the inode to find.
 *
 * Given a filepath, this function will traverse the filesystem to find the
 * file and return its inode. If the device number changes while traversing the
 * filesystem then dev is set to the new device number.
 *
 * Returns: The inode number if found, otherwise 0.
 */
int getino(int *dev, char *name)
{
	int ino;
	char buf[256];
	char *str = buf;
	strncpy(buf, name, 256);

	*dev = running->cwd->dev;
	ino = running->cwd->ino;
	if (*str == '/') {
		str++;
		*dev = root->dev;
		ino = root->ino;
	}

	char *tok[256];
	int len = tokenize(str, "/", tok);

	MINODE *mip = iget(*dev, ino);
	if (mip == NULL)
		return 0;

	for (int i = 0; i < len; i++) {
		if ((mip->inode.i_mode & 0xF000) != 0x4000) {
			// check if inode is a directory
			break;
		} else if (mip->ino == 2 && strcmp(tok[i], "..") == 0) {
			// going upstream out of mount point
			int i;
			for (i = 0; i < NMOUNT; i++) {
				if (mounttab[i].dev == mip->dev) {
					break;
				}
			}
			MINODE *m = mounttab[i].mounted_inode;

			// load the mount point
			iput(mip);
			mip = iget(m->dev, m->ino);

			// select parent of mount point
			*dev = mip->dev;
			ino = search(mip, "..");

			// load parent of mount point
			iput(mip);
			mip = iget(*dev, ino);
		} else if (ino = search(mip, tok[i]), ino == 0) {
			// find next inode number
			break;
		} else {
			// load next memory inode
			iput(mip);
			mip = iget(*dev, ino);

			if (mip->mounted) {
				// check if inode is mount point
				// if it is, then move into it
				*dev = mip->mountptr->dev;
				ino = 2;

				iput(mip);
				mip = iget(*dev, ino);
			}
		}
	}

	if (mip)
		iput(mip);

	return ino;
}

/*
 * Returns a pointer to the in memory INODE of
 * (dev, ino). The returned minode is unique (i.e.
 * only one copy in memory). The minode is also
 * locked to prevent use by anything else until
 * it is released or unlocked.
 */
MINODE *iget(int dev, int ino)
{
	MINODE *mip = 0;
	MINODE *tmp = 0;

	int i;
	for (i = 0; i < NMINODES; i++) {
		mip = &m_inodes[i];
		if (mip->ref_count > 0) {
			if (mip->dev == dev && mip->ino == ino) {
				mip->ref_count++;
				return mip;
			}
		} else if (!tmp) {
			tmp = mip;
		}
	}

	if (!tmp) {
		printf("minodes table is full\n");
		return 0;
	}

	// otherwise, use a new one
	mip = tmp;
	mip->mounted = 0;
	mip->dev = dev;
	mip->ino = ino;
	mip->ref_count = 1;

	inode_get_block(mip);

	return mip;
}

/*
 * Releases and unlocks a minode pointer by mip.
 * If using process is last to use mip (i.e.
 * ref_count = 0) then the INODE is written back
 * to the disk if it is dirty.
 */
void iput(MINODE *mip)
{
	if (--mip->ref_count > 0)
		return;

	if (mip->dirty) {
		inode_put_block(mip);
		mip->dirty = 0;
	}

	mip->ref_count = 0;
	mip->ino = 0;
	mip->dev = 0;
	mip->mounted = 0;
	mip->mountptr = 0;
}
