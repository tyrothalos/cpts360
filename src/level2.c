#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "project.h"

/* UNEXPORTED VARIABLES */

#define NOFT 100

static OFT oft[NOFT];

/* UNEXPORTED FUNCTIONS */

static int min(int a, int b)
{
	return (a < b) ? a : b;
}

static int find_proc_fd()
{
	for (int i = 0; i < NFD; i++)
		if (!running->fd[i])
			return i;
	return -1;
}

static OFT *find_oft(int ino)
{
	OFT *op = NULL;
	for (int i = 0; i < NOFT; i++) {
		if (oft[i].ref_count > 0) {
			if (oft[i].inodeptr && oft[i].inodeptr->ino == ino) {
				return &oft[i];
			}
		} else if (op == NULL) {
			op = &oft[i];
		}
	}
	return op;
}

unsigned int get_bno(int dev, unsigned int *blk)
{
	if (*blk == 0) {
		*blk = balloc(dev);
		unsigned int blank[BLOCK_SIZE];
		memset(blank, 0, BLOCK_SIZE);
		put_block(dev, *blk, (char *)blank);
	}
	return *blk;
}

/* EXPORTED FUNCTIONS */

/*
 * file_open:
 * @file: The path of the file to open.
 * @mode: The mode to open the file with.
 *
 * Returns: The file descriptor of the opened file if successful, otherwise -1.
 */
int file_open(char *file, int mode)
{
	int dev, ino, fd;
	OFT *op = NULL;
	MINODE *mip = NULL;

	if (mode < 0 || mode > 3) {
		printf("open: failed: invalid mode input\n");
	} else if ((ino = getino(&dev, file)) == 0) {
		printf("open: failed: path does not exist\n");
	} else if (mip = iget(dev, ino), mip == NULL) {
		printf("open: error: inode not found\n");
	} else if ((mip->inode.i_mode & 0xF000) != 0x8000) {
		printf("open: error: '%s' is not a regular file\n", file);
	} else if (fd = find_proc_fd(), fd < 0) {
		printf("open: failed: process table is full\n");
	} else if (op = find_oft(ino), op == NULL) {
		printf("open: failed: open file table is full\n");
	} else if (mode > 0 && op->mode > 0) {
		printf("open: failed: cannot open more than once for writing\n");
	} else if (op->ref_count > 0 && mode == 0 && op->mode == 0) {
		goto success;
	} else {
		switch (mode) {
			case 1:
				clear_blocks(mip); // W
			case 0:
			case 2:
				op->offset = 0; // R, W, RW
				break;
			case 3:
				op->offset = mip->inode.i_size; // APPEND
				break;
			default:
				assert(0);
		}
		time_t timer = time(0);
		switch (mode) {
			case 1:
			case 2:
			case 3:
				mip->inode.i_mtime = timer;
			case 0:
				mip->inode.i_atime = timer;
				break;
			default:
				assert(0);
		}
		mip->dirty = 1;
		op->mode = mode;
		op->inodeptr = mip;
success:
		op->ref_count++;
		running->fd[fd] = op;
		return fd;
	}

	if (mip)
		iput(mip);

	return -1;
}

/*
 * file_close:
 * @fd: The file descriptor of the file to close.
 *
 * Returns: 1 if the file is successfully closed, otherwise 0.
 */
int file_close(int fd)
{
	if (fd < 0 || fd >= NFD) {
		printf("close: fd is out of range\n");
	} else if (!running->fd[fd]) {
		printf("close: '%d' is not open\n", fd);
	} else {
		OFT *op = running->fd[fd];
		running->fd[fd] = 0;
		op->ref_count--;

		if (op->ref_count > 0)
			return 1;

		iput(op->inodeptr);
		op->inodeptr = 0;
		op->mode = 0;
		op->ref_count = 0;
		op->offset = 0;

		return 0;
	}
	return 1;
}

int file_lseek(int fd, int pos)
{
	OFT *op = NULL;
	if (fd < 0 || fd >= NFD) {
		printf("lseek: fd is out of range\n");
	} else if (!(op = running->fd[fd])) {
		printf("lseek: :failed: '%d' is not open\n", fd);
	} else if (pos >= op->inodeptr->inode.i_size) {
		printf("lseek: failed: position out of range\n");
	} else {
		int offset = op->offset;
		op->offset = pos;
		return offset;
	}
	return -1;
}

void shell_pfd()
{
	printf("fd  mode    offset  ref_count  INODE\n");
	printf("--  ----  --------  ---------  -----\n");

	int i;
	for (i = 0; i < NFD; i++) {
		if (running->fd[i]) {
			OFT *op = running->fd[i];
			printf("%2d  %4d  %8d   %8d  [%d, %d]\n",
					i, op->mode, op->offset, op->ref_count,
					op->inodeptr->dev, op->inodeptr->ino);
		}
	}
}

int file_read(int fd, char buf[], int n)
{
	OFT *op = NULL;
	if (fd < 0 || fd >= NFD) {
		printf("read: fd is out of range\n");
	} else if (!(op = running->fd[fd])) {
		printf("read: failed: '%d' is not open\n", fd);
	} else if (op->mode != 0 && op->mode != 2) {
		printf("read: failed: '%d' not open for reading\n", fd);
	} else {
		MINODE *mip = op->inodeptr;

		int bytes = mip->inode.i_size - op->offset;
		int count = 0;
		char *cq = buf;

		while ((n > 0) && (bytes > 0)) {
			int lbk   = op->offset / BLOCK_SIZE;
			int start = op->offset % BLOCK_SIZE;
			int blk;
			if (lbk < 12) {
				blk = mip->inode.i_block[lbk];
			} else if (lbk < 12 + 256) {
				unsigned int ibuf[256];
				get_block(mip->dev, mip->inode.i_block[12], (char *)ibuf);

				lbk -= 12;
				blk = ibuf[lbk];
			} else {
				unsigned int dbuf[256];
				get_block(mip->dev, mip->inode.i_block[13], (char *)dbuf);

				lbk -= (12 + 256);
				int dblk = dbuf[lbk / 256];
				int doff = lbk % 256;

				unsigned int ddbuf[256];
				get_block(mip->dev, dblk, (char *)ddbuf);

				blk = ddbuf[doff];
			}

			char tmp[BLOCK_SIZE];
			get_block(mip->dev, blk, tmp);

			char *cp = tmp + start;
			int remain = BLOCK_SIZE - start;
			remain = min(remain, n);
			remain = min(remain, bytes);
			memcpy(cq, cp, remain);

			n -= remain;
			bytes -= remain;

			cq += remain;
			count += remain;
			op->offset += remain;
		}
		return count;
	}
	return -1;
}

int file_write(int fd, char buf[], int n)
{
	OFT *op = NULL;
	if (fd < 0 || fd >= NFD) {
		printf("write: fd is out of range\n");
	} else if (op = running->fd[fd], op == NULL) {
		printf("write: failed: '%d' is not open\n", fd);
	} else if (op->mode == 0) {
		printf("write: failed: '%d' not open for writing\n", fd);
	} else {
		MINODE *mip = op->inodeptr;
		int count = 0;
		char *cq = buf;

		while (n > 0) {
			unsigned int lbk   = op->offset / BLOCK_SIZE;
			unsigned int start = op->offset % BLOCK_SIZE;

			int blk;
			if (lbk < 12) {
				blk = get_bno(mip->dev, &mip->inode.i_block[lbk]);
			} else if (lbk < 12 + 256) {
				unsigned int ibuf[256];
				unsigned int indirect = get_bno(mip->dev, &mip->inode.i_block[12]);
				get_block(mip->dev, indirect, (char *)ibuf);

				lbk -= 12;
				blk = get_bno(mip->dev, &ibuf[lbk]);
				put_block(mip->dev, indirect, (char *)ibuf);
			} else {
				unsigned int dbuf[256];
				unsigned int indirect = get_bno(mip->dev, &mip->inode.i_block[13]);
				get_block(mip->dev, indirect, (char *)dbuf);

				lbk -= (12 + 256);

				unsigned int ddbuf[256];
				unsigned int dblk = get_bno(mip->dev, &dbuf[lbk/256]);
				get_block(mip->dev, dblk, (char *)ddbuf);

				blk = get_bno(mip->dev, &ddbuf[lbk%256]);
				put_block(mip->dev, dblk, (char *)ddbuf);
				put_block(mip->dev, indirect, (char *)dbuf);
			}

			char tmp[BLOCK_SIZE];
			get_block(mip->dev, blk, tmp);
			char *cp = tmp + start;
			int remain = min(BLOCK_SIZE-start, n);
			memcpy(cp, cq, remain);
			put_block(mip->dev, blk, tmp);

			n -= remain;
			cq += remain;
			count += remain;
			op->offset += remain;
			if (op->offset > mip->inode.i_size)
				mip->inode.i_size = op->offset;
		}
		mip->dirty = 1;
		return count;
	}
	return -1;
}

void shell_cat(char *path)
{
	int fd;
	if ((fd = file_open(path, 0)) < 0) {
		printf("cat: failed: could not open file\n");
	} else {
		char buf[BLOCK_SIZE+1];
		buf[BLOCK_SIZE] = 0;

		int n;
		while ((n = file_read(fd, buf, BLOCK_SIZE)) > 0) {
			buf[n] = 0;
			for (int i = 0; i < n; i++) {
				if (buf[i] == '\n') {
					printf("\r\n");
				} else {
					putchar(buf[i]);
				}
			}
		}
	}

	if (fd >= 0)
		file_close(fd);
}

int file_cp(char *src, char *dst)
{
	int r = -1;
	int fd = -1, gd = -1;
	int dev, ino;

	if (fd = file_open(src, 0), fd < 0) {
		printf("cp: failed: cannot open '%s'\n", src);
	} else if (ino = getino(&dev, dst), ino == 0 && file_creat(dst) < 0) {
		printf("cp: failed: could not access '%s'\n", dst);
	} else if (gd = file_open(dst, 1), gd < 0) {
		printf("cp: failed: cannot open '%s'\n", dst);
	} else {
		char buf[BLOCK_SIZE+1];
		buf[BLOCK_SIZE] = 0;

		int n = 0;
		while ((n = file_read(fd, buf, BLOCK_SIZE)) > 0) {
			buf[n] = 0;
			file_write(gd, buf, n);
		}

		r = 0;
	}

	if (fd >= 0)
		file_close(fd);
	if (gd >= 0)
		file_close(gd);

	return r;
}

int file_mv(char *src, char *dst)
{
	int r = -1;
	int sdev, sino;
	int ddev, dino;

	if (sino = getino(&sdev, src), sino == 0) {
		printf("mv: failed: source does not exist\n");
	} else if (dino = getino(&ddev, dst), dino != 0 && file_unlink(dst) < 0) {
		printf("mv: failed: existing file at destination cannot be removed\n");
	} else if ((sdev == ddev ? file_link(src, dst) : file_cp(src, dst)) < 0) {
		printf("mv: failed: cannot create file at destination\n");
	} else if (file_unlink(src) < 0) {
		printf("mv: failed: cannot remove file at source\n");
		file_unlink(dst);
	} else {
		r = 0;
	}

	return r;
}

