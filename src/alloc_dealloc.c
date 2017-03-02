#include <stdio.h>
#include <stdlib.h>
#include <ext2fs/ext2_fs.h>

#include "alloc_dealloc.h"
#include "type.h"
#include "util.h"

/*
 * tst_bit:
 * @buf: The buffer of bytes.
 * @bit: Position of the bit to check.
 * 
 * Checks if the bit at the given location in the 
 * given buffer is set.
 * 
 * Returns: The value of the bit.
 */
static int tst_bit(char buf[], int bit)
{
	int i = bit / 8;
	int j = bit % 8;
	return buf[i] & (1 << j);
}

static void set_bit(char buf[], int bit)
{
	int i = bit / 8;
	int j = bit % 8;
	buf[i] |= (1 << j);
}

static void clr_bit(char buf[], int bit)
{
	int i = bit / 8;
	int j = bit % 8;
	buf[i] &= ~(1 << j);
}

/*
 * add_inodes:
 * @dev: The file descriptor for the device.
 * @amt: The amount of free inodes to add.
 */
static void add_inodes(int dev, int amt)
{
	char buf[BLOCK_SIZE];

	get_block(dev, SUPER_BLOCK, buf);
	SUPER *s = (SUPER *)buf;
	s->s_free_inodes_count += amt;
	put_block(dev, SUPER_BLOCK, buf);
	
	get_block(dev, GD_BLOCK, buf);
	GD *gd = (GD *)buf;
	gd->bg_free_inodes_count += amt;
	put_block(dev, GD_BLOCK, buf);
}

static void add_blocks(int dev, int amt)
{
	char buf[BLOCK_SIZE];

	get_block(dev, SUPER_BLOCK, buf);
	SUPER *s = (SUPER *)buf;
	s->s_free_blocks_count += amt;
	put_block(dev, SUPER_BLOCK, buf);
	
	get_block(dev, GD_BLOCK, buf);
	GD *gd = (GD *)buf;
	gd->bg_free_blocks_count += amt;
	put_block(dev, GD_BLOCK, buf);
}

/*
 * Allocates a new inode and returns its ino
 */
int ialloc(int dev)
{
	char buf1[BLOCK_SIZE];
	char buf2[BLOCK_SIZE];
	char map[BLOCK_SIZE];

	get_block(dev, SUPER_BLOCK, buf1);
	get_block(dev, GD_BLOCK, buf2);

	SUPER *s = (SUPER *)buf1;
	GD *gd = (GD *)buf2;
	
	get_block(dev, gd->bg_inode_bitmap, map);

	int i;
	for (i = 0; i < s->s_inodes_count; i++) {
		if (tst_bit(map, i) == 0) {
			set_bit(map, i);
			put_block(dev, gd->bg_inode_bitmap, map);
			add_inodes(dev, -1);

			return i + 1;
		}
	}
	printf("ialloc(): no more free inodes\n");

	return 0;
}

int balloc(int dev)
{
	char buf1[BLOCK_SIZE];
	char buf2[BLOCK_SIZE];
	char bmap[BLOCK_SIZE];

	get_block(dev, SUPER_BLOCK, buf1);
	get_block(dev, GD_BLOCK, buf2);

	SUPER *s = (SUPER *)buf1;
	GD *gd = (GD *)buf2;
	
	get_block(dev, gd->bg_block_bitmap, bmap);

	int i;
	for (i = 0; i < s->s_blocks_count; i++) {
		if (tst_bit(bmap, i) == 0) {
			set_bit(bmap, i);
			put_block(dev, gd->bg_block_bitmap, bmap);
			add_blocks(dev, -1);

			return i + 1;
		}
	}
	printf("balloc(): no more free blocks\n");

	return 0;
}

int idealloc(int dev, int ino)
{
	int r = 0;
	ino--;

	char buf1[BLOCK_SIZE];
	char buf2[BLOCK_SIZE];
	char map[BLOCK_SIZE];

	get_block(dev, SUPER_BLOCK, buf1);
	get_block(dev, GD_BLOCK, buf2);

	SUPER *s = (SUPER *)buf1;
	GD *gd = (GD *)buf2;
	
	get_block(dev, gd->bg_inode_bitmap, map);

	if (ino >= s->s_inodes_count) {
		printf("idealloc(): ino %d out of range %d\n", ino, s->s_inodes_count);
		r = -1;
	} else if (tst_bit(map, ino) == 0) {
		printf("idealloc(): ino %d is not allocated\n", ino);
		r = -2;
	} else {
		clr_bit(map, ino);
		put_block(dev, gd->bg_inode_bitmap, map);
		add_inodes(dev, 1); // must be done AFTER putting
	}
	return r;
}

int bdealloc(int dev, int bno)
{
	int r = 0;
	bno--;

	char buf1[BLOCK_SIZE];
	char buf2[BLOCK_SIZE];
	char bmap[BLOCK_SIZE];
	
	get_block(dev, SUPER_BLOCK, buf1);
	get_block(dev, GD_BLOCK, buf2);
	
	SUPER *s = (SUPER *)buf1;
	GD *gd = (GD *)buf2;
	
	get_block(dev, gd->bg_block_bitmap, bmap);
	
	if (bno >= s->s_blocks_count) {
		printf("bdealloc(): bno %d out of range %d\n", bno, s->s_blocks_count);
		r = -1;
	} else if (tst_bit(bmap, bno) == 0) {
		printf("bdealloc(): bno %d is not allocated\n", bno);
		r = -2;
	} else {
		clr_bit(bmap, bno);
		put_block(dev, gd->bg_block_bitmap, bmap);
		add_blocks(dev, 1);
	}
	return r;
}

