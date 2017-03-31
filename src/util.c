#include <stdio.h>
#include <string.h>
#include <libgen.h>
#include <unistd.h>

#include "project.h"

/* GENERIC BLOCK GETTER */

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

/* INODE UTILITY */

static void inode_get_block(MINODE *mip)
{
	int dev = mip->dev;
	int ino = mip->ino;

	// get inode's group to find where inode_table is
	char buf[BLOCK_SIZE];
	get_block(dev, GD_BLOCK, buf);
	GD *gd = (GD *)buf;

	int inodes_per_block = BLOCK_SIZE / INODE_SIZE;
	int blk = ((ino - 1) / inodes_per_block) + gd->bg_inode_table;
	int off = ((ino - 1) % inodes_per_block);
	int inode_ratio = INODE_SIZE / 128;

	// get block inode is in
	get_block(mip->dev, blk, buf);
	// select target inode
	INODE *tmp = (INODE *)buf + off*inode_ratio;
	// copy to minode
	mip->inode = *tmp;
}

static void inode_put_block(MINODE *mip)
{
	int dev = mip->dev;
	int ino = mip->ino;

	// get inode's group to find where inode_table is
	char buf[BLOCK_SIZE];
	get_block(dev, GD_BLOCK, buf);
	GD *gd = (GD *)buf;

	int inodes_per_block = BLOCK_SIZE / INODE_SIZE;
	int blk = ((ino - 1) / inodes_per_block) + gd->bg_inode_table;
	int off = ((ino - 1) % inodes_per_block);
	int inode_ratio = INODE_SIZE / 128;

	// get block inode is in
	get_block(mip->dev, blk, buf);
	// select target inode
	INODE *tmp = (INODE *)buf + off*inode_ratio;
	// copy minode to inode
	*tmp = mip->inode;
	// put back to disk
	put_block(mip->dev, blk, buf);
}

int tokenize(char *path, char *delim, char *buf[])
{
	int i = 0;
	char *tmp = strtok(path, delim);
	while (tmp) {
		buf[i++] = tmp;
		tmp = strtok(NULL, delim);
	}
	return i;
}

/*
 * Handles the parsing of a path into dirname and basename as well
 * as finding the inode of the parent, if it exists.
 */
int parseargs(char *path, int *dev, char **parent, char **child)
{
	char tmp1[256], tmp2[256];
	strcpy(tmp1, path);
	strcpy(tmp2, path);

	*parent = strdup(dirname(tmp1));
	*child = strdup(basename(tmp2));

	int ino;
	if (strcmp(*parent, ".") == 0) {
		*dev = running->cwd->dev;
		ino = running->cwd->ino;
	} else {
		ino = getino(dev, *parent);
	}
	return ino;
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

static void clear_direct(int dev, unsigned int *blocks, int n)
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

/*
 * Given a filepath, this function will traverse the
 * filesystem to find the file and return its inode.
 * If the file cannot be found, then 0 is returned.
 *
 * Returns inode number of pathname. If the device
 * number changes while traversing the filesystem
 * then dev is set to the new device number.
 */
int getino(int *dev, char *name)
{
	MINODE *mip = NULL;
	char *str, buffer[256];
	strncpy(buffer, name, 256);
	str = buffer;

	int ino = running->cwd->ino;
	*dev    = running->cwd->dev;
	if (*str == '/') {
		str++;
		ino  = root->ino;
		*dev = root->dev;
	}
	char *tok[256];
	int len = tokenize(str, "/", tok);

	int i, r = 0;
	for (i = 0; i < len; i++) {
		if (mip = iget(*dev, ino), mip == NULL) {
			r = -1;
		} else if ((mip->inode.i_mode & 0xF000) != 0x4000) {
			r = -2;
		} else if (mip->ino == 2
				&& mip->dev != root->dev
				&& strcmp(tok[i], "..") == 0) {
			int i;
			for (i = 0; i < NMOUNT; i++) {
				if (mounttab[i].dev == mip->dev) {
					break;
				}
			}

			iput(mip);
			MINODE *m = mounttab[i].mounted_inode;
			mip = iget(m->dev, m->ino);

			ino  = search(mip, "..");
			*dev = m->dev;
		} else if ((ino = search(mip, tok[i])) == 0) {
			r = -3;
		} else {
			// check to see if this ino is mounted
			iput(mip);
			mip = iget(*dev, ino);

			if (mip->mounted) {
				MOUNT *mount = mip->mountptr;
				iput(mip);
				mip = iget(mount->dev, 2);

				ino  = 2;
				*dev = mount->dev;
			}
		}

		if (mip)
			iput(mip);
		if (r < 0)
			return 0;
	}

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
}

