#include <stdio.h>
#include <ext2fs/ext2_fs.h>

#include "project.h"

/*
 * tst_bit:
 * @buf: The buffer of bytes.
 * @bit: Position of the bit to check.
 *
 * Checks if the bit at the given location in the given buffer is set.
 *
 * Returns: The value of the bit.
 */
static int tst_bit(char buf[], int bit)
{
	int i = bit / 8;
	int j = bit % 8;
	return buf[i] & (1 << j);
}

/*
 * set_bit:
 * @buf: The buffer of bytes.
 * @bit: Position of the bit to set.
 *
 * Sets the bit at the given location in the given buffer to 1.
 */
static void set_bit(char buf[], int bit)
{
	int i = bit / 8;
	int j = bit % 8;
	buf[i] |= (1 << j);
}

/*
 * clr_bit:
 * @buf: The buffer of bytes.
 * @bit: Position of the bit to set.
 *
 * Sets the bit at the given location in the given buffer to 0.
 */
static void clr_bit(char buf[], int bit)
{
	int i = bit / 8;
	int j = bit % 8;
	buf[i] &= ~(1 << j);
}

/*
 * ialloc:
 * @dev: The device to allocate an inode on.
 *
 * Allocates a new inode.
 *
 * Returns: 0 if the allocation failed, otherwise the new inode number.
 */
int ialloc(int dev)
{
	char bmap[BLOCK_SIZE];

	SUPER s = get_super_block(dev);
	GD    g = get_group_block(dev);
	get_block(dev, g.bg_inode_bitmap, bmap);

	for (int i = 0; i < s.s_inodes_count; i++) {
		if (tst_bit(bmap, i) == 0) {
			set_bit(bmap, i);
			s.s_free_inodes_count--;
			g.bg_free_inodes_count--;

			put_block(dev, g.bg_inode_bitmap, bmap);
			put_group_block(dev, g);
			put_super_block(dev, s);

			return i + 1;
		}
	}
	printf("%s: no more free inodes\n", __func__);

	return 0;
}

/*
 * balloc:
 * @dev: The device to allocate a block on.
 *
 * Allocates a new empty block.
 *
 * Returns: 0 if the allocation failed, otherwise the new block number.
 */
int balloc(int dev)
{
	char bmap[BLOCK_SIZE];

	SUPER s = get_super_block(dev);
	GD    g = get_group_block(dev);
	get_block(dev, g.bg_block_bitmap, bmap);

	for (int i = 0; i < s.s_blocks_count; i++) {
		if (tst_bit(bmap, i) == 0) {
			set_bit(bmap, i);
			s.s_free_blocks_count--;
			g.bg_free_blocks_count--;

			put_block(dev, g.bg_block_bitmap, bmap);
			put_group_block(dev, g);
			put_super_block(dev, s);

			return i + 1;
		}
	}
	printf("%s: no more free blocks\n", __func__);

	return 0;
}

/*
 * idealloc:
 * @dev: The device to deallocate an inode on.
 * @ino: The inode number to deallocate.
 *
 * Deallocates an inode.
 *
 * Returns: 0 if the deallocation succeeded,
 *         -1 if the given inode number is not valid,
 *         -2 if the given inode number is not allocated.
 */
int idealloc(int dev, int ino)
{
	int r = 0;
	char bmap[BLOCK_SIZE];
	ino--;

	SUPER s = get_super_block(dev);
	GD    g = get_group_block(dev);
	get_block(dev, g.bg_inode_bitmap, bmap);

	if (ino < 0 || ino >= s.s_inodes_count) {
		printf("%s: ino %d out of range %d\n", __func__, ino, s.s_inodes_count);
		r = -1;
	} else if (tst_bit(bmap, ino) == 0) {
		printf("%s: ino %d is not allocated\n", __func__, ino);
		r = -2;
	} else {
		clr_bit(bmap, ino);
		s.s_free_inodes_count++;
		g.bg_free_inodes_count++;

		put_block(dev, g.bg_inode_bitmap, bmap);
		put_group_block(dev, g);
		put_super_block(dev, s);
	}
	return r;
}

/*
 * bdealloc:
 * @dev: The device to deallocate a block on.
 * @ino: The block number to deallocate.
 *
 * Deallocates a block.
 *
 * Returns: 0 if the deallocation succeeded,
 *         -1 if the given block number is not valid,
 *         -2 if the given block number is not allocated.
 */
int bdealloc(int dev, int bno)
{
	int r = 0;
	char bmap[BLOCK_SIZE];
	bno--;

	SUPER s = get_super_block(dev);
	GD    g = get_group_block(dev);
	get_block(dev, g.bg_block_bitmap, bmap);

	if (bno < 0 || bno >= s.s_blocks_count) {
		printf("%s: bno %d out of range %d\n", __func__, bno, s.s_blocks_count);
		r = -1;
	} else if (tst_bit(bmap, bno) == 0) {
		printf("%s: bno %d is not allocated\n", __func__, bno);
		r = -2;
	} else {
		clr_bit(bmap, bno);
		s.s_free_blocks_count++;
		g.bg_free_blocks_count++;

		put_block(dev, g.bg_block_bitmap, bmap);
		put_group_block(dev, g);
		put_super_block(dev, s);
	}
	return r;
}

