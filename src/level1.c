#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libgen.h>
#include <time.h>

#include "alloc_dealloc.h"
#include "global.h"
#include "level1.h"
#include "type.h"
#include "util.h"

static void insert_entry(MINODE *mip, int ino, char *name)
{
	int i;
	for (i = 0; i < 12; i++) {
		if (mip->inode.i_block[i+1] == 0) {
			break;
		}
	}
	
	char buf[BLOCK_SIZE];
	get_block(mip->dev, mip->inode.i_block[i], buf);
	
	int  len = 0;
	char *cp = buf;
	DIR  *dp = (DIR *)cp;

	// go to the end of the block
	while (dp->rec_len + len < BLOCK_SIZE) {
		if (dp->rec_len < 1) {
			printf("dir error: %s, name_len: %d, inode: %d, rec_len: %d\n",
					dp->name, dp->name_len, dp->inode, dp->rec_len);
			return;
		}
		len += dp->rec_len;
		cp  += dp->rec_len;
		dp   = (DIR *)cp;
	}

	// get length of current last entry
	int idea_len = 4 * ((11 + dp->name_len) / 4);
	if ((BLOCK_SIZE - len) >= idea_len) {
		// if there is enough space, add
		// it to the end and update the rec_len
		dp->rec_len = idea_len;

		len += dp->rec_len;
		cp  += dp->rec_len;
		dp   = (DIR *)cp;

		dp->inode    = ino;
		dp->name_len = strlen(name);
		strncpy(dp->name, name, dp->name_len);
		dp->rec_len  = BLOCK_SIZE - len;

		put_block(mip->dev, mip->inode.i_block[i], buf);
	} else {
		// if there is not enough space, then 
		// alloc a new block and use that
		int bno = balloc(mip->dev);
		mip->inode.i_block[++i] = bno;
		get_block(mip->dev, mip->inode.i_block[i], buf);

		cp = buf;
		dp = (DIR *)cp;

		dp->inode = ino;
		dp->name_len = strlen(name);
		strncpy(dp->name, name, dp->name_len);
		dp->rec_len = BLOCK_SIZE;
		
		mip->inode.i_size += BLOCK_SIZE;
		put_block(mip->dev, mip->inode.i_block[i], buf);
	}
}

static void help_mknod(int dev, int ino, int bno, 
		int mode, int size, int links)
{
	MINODE *mip = iget(dev, ino);
	INODE  *ip  = &mip->inode;

	ip->i_mode = mode;
	ip->i_size = size;
	ip->i_links_count = links;
	
	ip->i_uid  = running->uid;
	ip->i_gid  = running->gid;
	
	time_t timer = time(0);
	ip->i_ctime = timer;
	ip->i_atime = timer;
	ip->i_mtime = timer;

	ip->i_blocks = 2;
	ip->i_block[0] = bno;

	int i;
	for (i = 1; i < 15; i++) {
		ip->i_block[i] = 0;
	}

	mip->dirty = 1;
	iput(mip);
}

static void remove_entry(MINODE *mip, char *name)
{
	char buf[BLOCK_SIZE], tmp[256];
	
	int i;
	for (i = 0; i < 12; i++) {
		if (mip->inode.i_block[i] == 0)
			continue;

		get_block(mip->dev, mip->inode.i_block[i], buf);
		
		int  len = 0;
		int  pre = 0;
		char *cp = buf;
		DIR  *dp = (DIR *)cp;

		while (len < BLOCK_SIZE) {
			strncpy(tmp, dp->name, dp->name_len);
			tmp[dp->name_len] = 0;

			if (strcmp(tmp, name) != 0) {
				len += dp->rec_len;
				pre  = dp->rec_len;
				cp  += dp->rec_len;
				dp   = (DIR *)cp;
				continue;
			}

			if (dp->rec_len == BLOCK_SIZE) {
				// case: only entry in block
				memset(buf, 0, BLOCK_SIZE);
				put_block(mip->dev, mip->inode.i_block[i], buf);
				bdealloc(mip->dev, mip->inode.i_block[i]);
				mip->inode.i_size -= BLOCK_SIZE;

				int j;
				for (j = i; j < 11; j++) {
					mip->inode.i_block[j] = mip->inode.i_block[j+1];
				}
				mip->inode.i_block[11] = 0;
			} else if (len + dp->rec_len == BLOCK_SIZE) {
				// case: last entry in block
				len -= pre;
				cp  -= pre;
				dp   = (DIR *)cp;

				dp->rec_len = BLOCK_SIZE - len;
				put_block(mip->dev, mip->inode.i_block[i], buf);
			} else { 
				// case: somewhere in middle of block
				int offset = dp->rec_len;
				char *cn = cp + dp->rec_len;
				DIR  *dn = (DIR *)cn;

				while (len + offset + dn->rec_len < BLOCK_SIZE) {
					dp->inode = dn->inode;
					dp->name_len = dn->name_len;
					strncpy(dp->name, dn->name, dn->name_len);
					dp->rec_len = dn->rec_len;

					// set to next position to overwrite
					cp  += dn->rec_len;
					dp   = (DIR *)cp;
			
					// set to next values to use for overwrite
					len += dn->rec_len;
					cn  += dn->rec_len;
					dn   = (DIR *)cn;

					if (dn->rec_len == 0) {
						printf("error: %s, %d, %d\n", dn->name, dn->name_len, dn->inode);
						return;
					}
				}
				// copy last entries values over, but
				// set rec_len to take up rest of space
				dp->inode = dn->inode;
				dp->name_len = dn->name_len;
				strncpy(dp->name, dn->name, dn->name_len);
				dp->rec_len = BLOCK_SIZE - len;

				put_block(mip->dev, mip->inode.i_block[i], buf);
			}
			mip->dirty = 1;
			return;
		}
	}
}

/* LEVEL ONE COMMANDS */

/* LS COMMAND */

static void print_dir_entry(int dev, int ino, char *name)
{
	char *perms = "xwrxwrxwr";
	
	MINODE *mip = iget(dev, ino);
	if (!mip)
		return;

	INODE *inode = &mip->inode;
	int mode = inode->i_mode & 0xF000;
	if (mode == 0x8000) putchar('-');
	if (mode == 0x4000) putchar('d');
	if (mode == 0xA000) putchar('l');

	int i;
	for (i = 8; i >= 0; i--) {
		if (inode->i_mode & (1 << i)) {
			putchar(perms[i]);
		} else {
			putchar('-');
		}
	}

	printf("%4d ", (int)inode->i_links_count);
	printf("%4d ", (int)inode->i_uid);
	printf("%4d ", (int)inode->i_gid);
	printf("%8d ", (int)inode->i_size);

	char buffer[13];
	time_t inode_time = inode->i_mtime;
	struct tm *timeinfo = localtime(&inode_time);
	strftime(buffer, 13, "%b %e %H:%M", timeinfo);
	printf("%s ", buffer);

	printf("%s", name);
	if (mode == 0x4000)
		putchar('/');
	if (mode == 0xA000)
		printf(" -> %s", (char *)(inode->i_block));

	printf("\n");
	iput(mip);
}

void ls_file(MINODE *mip, char *name)
{
	char buf[BLOCK_SIZE];
	get_block(mip->dev, mip->inode.i_block[0], buf);
	
	char *cp = buf;
	DIR  *dp = (DIR *)buf;

	char tmp[256];
	while (cp < buf + BLOCK_SIZE) {
		strncpy(tmp, dp->name, dp->name_len);
		tmp[dp->name_len] = 0;

		if (strcmp(tmp, name) == 0) {
			print_dir_entry(mip->dev, dp->inode, tmp);
			return;
		}
		
		if (!dp->rec_len)
				break;	
			
		cp += dp->rec_len;
		dp = (DIR *)cp;
	}
}

void ls_dir(MINODE *mip)
{
	char buf[BLOCK_SIZE];
	get_block(mip->dev, mip->inode.i_block[0], buf);
	
	char *cp = buf;
	DIR  *dp = (DIR *)buf;

	char tmp[256];
	while (cp < buf + BLOCK_SIZE) {
		strncpy(tmp, dp->name, dp->name_len);
		tmp[dp->name_len] = 0;

		print_dir_entry(mip->dev, dp->inode, tmp);
		
		if (!dp->rec_len)
				break;	
			
		cp += dp->rec_len;
		dp = (DIR *)cp;
	}
}

void my_ls()
{
	int dev, pino, cino;
	char *parent = NULL, *child = NULL;
	MINODE *pmip = NULL, *cmip = NULL;
	
	char *path = (myargc > 1) ? myargs[1] : ".";
	
	if ((pino = parseargs(path, &dev, &parent, &child)) == 0) {
		printf("ls: failed: path does not exist\n");	
	} else if (pmip = iget(dev, pino), pmip == NULL) {
		printf("ls: error: inode '%d' not found\n", pino);
	} else if ((pmip->inode.i_mode & 0xF000) != 0x4000) {
		printf("ls: failed: '%s' is not a directory\n", parent);
	} else if ((cino = search(pmip, child)) == 0) {
		printf("ls: failed: '%s' does not exist\n", child);
	} else if (cmip = iget(dev, cino), cmip == NULL) {
		printf("ls: error: inode '%d' not found\n", cino);
	} else if ((cmip->inode.i_mode & 0xF000) != 0x4000) {
		ls_file(pmip, child);
	} else {
		ls_dir(cmip);
	}

	if (pmip)
		iput(pmip);
	if (cmip)
		iput(cmip);
	if (parent)
		free(parent);
	if (child)
		free(child);
}

/* CD COMMAND */

void my_cd()
{
	int dev, ino;
	MINODE *mip = NULL;

	if (myargc == 1) {
		iput(running->cwd);
		running->cwd= root;
		root->ref_count++;
		return;
	} else if ((ino = getino(&dev, myargs[1])) == 0) {
		printf("cd: failed: path does not exist\n");
	} else if (mip = iget(dev, ino), mip == NULL) {
		printf("cd: error: inode not found\n");
	} else if ((mip->inode.i_mode & 0xF000) != 0x4000) {
		printf("cd: failed: '%s' is not a directory\n", myargs[1]);
	} else {
		iput(running->cwd);
		running->cwd = mip;
		return;
	}
	
	if (mip)
		iput(mip);
}

/* PWD COMMAND */

static int print_entry_name(MINODE *mip, int ino)
{
	char buf[BLOCK_SIZE], tmp[256];
	get_block(mip->dev, mip->inode.i_block[0], buf);
	
	char *cp = buf;
	DIR  *dp = (DIR *)buf;

	while ((cp < buf + BLOCK_SIZE) && (dp->rec_len > 0)) {
		strncpy(tmp, dp->name, dp->name_len);
		tmp[dp->name_len] = 0;
		
		if (ino == dp->inode) {
			printf("%s", tmp);
			return 1;
		}

		cp += dp->rec_len;
		dp = (DIR *)cp;
	}
	return 0;
}

static void help_pwd(MINODE *mip)
{
	if (mip == root)
		return;

	int ino = search(mip, "..");
	MINODE *parent = iget(mip->dev, ino);
	
	help_pwd(parent);
	printf("/");
	print_entry_name(parent, mip->ino);

	iput(parent);
}

void my_pwd()
{
	if (running->cwd == root) {
		printf("/\n");
	} else {
		help_pwd(running->cwd);
		printf("\n");
	}	
}

/* MKDIR COMMAND */

static void help_mkdir(MINODE *parent, char *child)
{
	int ino = ialloc(parent->dev);
	int bno = balloc(parent->dev);

	help_mknod(parent->dev, ino, bno, 0x41ED, BLOCK_SIZE, 2);

	// ENTERING '.' AND '..' ENTRIES START
	char buf[BLOCK_SIZE];
	memset(buf, 0, BLOCK_SIZE);
	
	int  len = 0;
	char *cp = buf;
	DIR  *dp = (DIR *)cp;

	dp->inode    = ino;
	dp->name_len = 1;
	strncpy(dp->name, ".", 1);
	dp->rec_len  = 12;

	len += dp->rec_len;
	cp  += dp->rec_len;
	dp   = (DIR *)cp;

	dp->inode    = parent->ino;
	dp->name_len = 2;
	strncpy(dp->name, "..", 2);
	dp->rec_len  = BLOCK_SIZE - 12;
	
	put_block(parent->dev, bno, buf);
	// ENTERING '.' AND '..' ENTRIES FINISH

	insert_entry(parent, ino, child);
}

int file_mkdir(char *path)
{
	int r = -1;
	int dev, ino;
	char *parent = NULL, *child = NULL;	
	MINODE *mip = NULL;	
	
	if ((ino = parseargs(path, &dev, &parent, &child)) < 1) {
		printf("mkdir: failed: path does not exist\n");
	} else if (mip = iget(dev, ino), mip == NULL) {
		printf("mkdir: failed: inode not found\n");
	} else if ((mip->inode.i_mode & 0xF000) != 0x4000) {
		printf("mkdir: %s: Not a directory\n", path);
	} else if (search(mip, child) != 0) {
		printf("mkdir: cannot create directory '%s': File exists\n", child);	
	} else {
		help_mkdir(mip, child);
		
		mip->inode.i_atime = time(0);
		mip->inode.i_links_count++;
		mip->dirty = 1;

		r = 0;
	}

	if (mip)
		iput(mip);
	if (parent)
		free(parent);
	if (child)
		free(child);

	return r;
}

void my_mkdir()
{	
	if (myargc == 1) {
		printf("mkdir: missing operand\n");
	} else {
		file_mkdir(myargs[1]);
	}
}

/* CREAT COMMAND */

int file_creat(char *path)
{
	int r = -1;
	int dev, ino;
	char *parent = NULL, *child = NULL;
	MINODE *mip = NULL;
	
	if ((ino = parseargs(path, &dev, &parent, &child)) == 0) {
		printf("creat: failed: path does not exist");
	} else if (mip = iget(dev, ino), mip == NULL) {
		printf("creat: failed: inode not found");
	} else if ((mip->inode.i_mode & 0xF000) != 0x4000) {
		printf("creat: %s: Not a directory\n", path);
	} else if (search(mip, child) != 0) {
		printf("creat: cannot create file '%s': File exists\n", child);	
	} else {
		int inum = ialloc(mip->dev);
		int bnum = balloc(mip->dev);	
		
		help_mknod(mip->dev, inum, bnum, 0x81A4, 0, 1);
		char buf[BLOCK_SIZE];
		memset(buf, 0, BLOCK_SIZE);
		put_block(mip->dev, bnum, buf);
		insert_entry(mip, inum, child);
		
		mip->inode.i_atime = time(0);
		mip->dirty = 1;

		r = 0;
	}

	if (mip)
		iput(mip);
	if (parent)
		free(parent);
	if (child)
		free(child);
	
	return r;
}

void my_creat()
{
	if (myargc == 1) {
		printf("creat: missing operand\n");
	} else {
		file_creat(myargs[1]);
	}
}

/* RMDIR COMMAND */

static int dir_is_empty(MINODE *mip)
{
	if (mip->inode.i_links_count > 2)
		return 0;

	char buf[BLOCK_SIZE];

	int i;
	for (i = 0; i < 12; i++) {
		if (mip->inode.i_block[i] == 0)
			continue;

		get_block(mip->dev, mip->inode.i_block[i], buf);
		
		int  len = 0;
		char *cp = buf;
		DIR  *dp = (DIR *)cp;
		
		while (len < BLOCK_SIZE) {
			dp->name[dp->name_len] = 0;
			
			if (!strcmp(dp->name, ".") && !strcmp(dp->name, ".."))
				return 0;

			len += dp->rec_len;
			cp  += dp->rec_len;
			dp   = (DIR *)cp;
		}
	}

	return 1;
}

int file_rmdir(char *path)
{
	int r = -1;
	int dev, pino, cino;
	char *parent = NULL, *child = NULL;
	MINODE *pmip = NULL, *cmip = NULL;

	if ((pino = parseargs(path, &dev, &parent, &child)) == 0) {
		printf("rmdir: failed: path does not exist\n");
	} else if (pmip = iget(dev, pino), pmip == NULL) {
		printf("rmdir: error: inode '%d' not found\n", pino);
	} else if ((pmip->inode.i_mode & 0xF000) != 0x4000) {
		printf("rmdir: failed: '%s' is not a directory\n", parent);
	} else if ((cino = search(pmip, child)) == 0) {
		printf("rmdir: failed: no such directory '%s'\n", child);
	} else if (cmip = iget(pmip->dev, cino), cmip == NULL) {
		printf("rmdir: error: inode '%d' not found\n", cino);
	} else if (cmip->mounted) {
		printf("rmdir: error: cannot rmdir a mount point\n");
	} else if ((cmip->inode.i_mode & 0xF000) != 0x4000) {
		printf("rmdir: failed: '%s' is not a directory\n", child);
	} else if (!has_perm(cmip, 2)) {
		printf("rmdir: failed: wrong permissions\n");
	} else if (cmip->ref_count > 1) {
		printf("rmdir: failed: '%s' is busy\n", child);
	} else if (!dir_is_empty(cmip)) {
		printf("rmdir: failed: '%s' is not empty\n", child);
	} else {
		remove_entry(pmip, child);
		int i;
		for (i = 0; i < 12; i++) {
			if (cmip->inode.i_block[i] != 0) {
				bdealloc(cmip->dev, cmip->inode.i_block[i]);
			}
		}
		idealloc(cmip->dev, cmip->ino);

		time_t timer = time(0);
		pmip->inode.i_atime = timer;
		pmip->inode.i_mtime = timer;
		pmip->inode.i_links_count--;

		cmip->dirty = 1;
		pmip->dirty = 1;

		r = 0;
	}

	if (cmip)
		iput(cmip);
	if (pmip)
		iput(pmip);
	if (parent)
		free(parent);
	if (child)
		free(child);

	return r;
}

void my_rmdir()
{
	if (myargc == 1) {
		printf("rmdir: missing operand\n");
	} else {
		file_rmdir(myargs[1]);
	}
}

/* RM COMMAND */

void my_rm()
{
	my_unlink();
}

/* LINK COMMAND */

int file_link(char *src, char *dst)
{
	int r = -1;
	int sdev, tdev, sino, tino;
	char *parent = NULL, *child = NULL;
	MINODE *smip = NULL, *tmip = NULL;

	if ((sino = getino(&sdev, src)) == 0) {
		printf("link: failed: source path does not exist\n");
	} else if (smip = iget(sdev, sino), smip == NULL) {
		printf("link: failed: source inode not found\n");
	} else if ((smip->inode.i_mode & 0xF000) == 0x4000) {
		printf("link: failed: cannot link a directory\n");
	} else if ((tino = parseargs(dst, &tdev, &parent, &child)) == 0) {
		printf("link: failed: destination path does not exist\n");
	} else if (sdev != tdev) {
		printf("link: failed: both paths must be on the same device\n");
	} else if (tmip = iget(tdev, tino), tmip == NULL) {
		printf("link: error: target inode not found\n");
	} else if ((tmip->inode.i_mode & 0xF000) != 0x4000) {
		printf("link: failed: '%s' is not a directory\n", parent);
	} else if (search(tmip, child) != 0) {
		printf("link: failed: '%s' already exists\n", child);
	} else {
		insert_entry(tmip, sino, child);
		smip->inode.i_links_count++;
		
		smip->dirty = 1;
		tmip->dirty = 1;

		r = 0;
	}

	if (smip)
		iput(smip);
	if (tmip)
		iput(tmip);
	if (parent)
		free(parent);
	if (child)
		free(child);

	return r;
}

void my_link()
{	
	if (myargc < 3) {
		printf("link: missing operand\n");
	} else {
		file_link(myargs[1], myargs[2]);
	}
}

/* SYMLINK COMMAND */

void my_symlink()
{
	int sdev, tdev, sino, tino;
	char *parent = NULL, *child = NULL;
	MINODE *smip = NULL, *tmip = NULL;

	if (myargc < 3) {
		printf("symlink: missing operand\n");
	} else if (strlen(myargs[1]) >= 60) {
		printf("symlink: failed: source path too long\n");
	} else if ((sino = getino(&sdev, myargs[1])) == 0) {
		printf("symlink: failed: source path does not exist\n");
	} else if (smip = iget(sdev, sino), smip == NULL) {
		printf("symlink: error: source inode not found\n");
	} else if ((smip->inode.i_mode & 0xF000) == 0xA000) {
		printf("symlink: failed: must be directory or regular file\n");
	} else if ((tino = parseargs(myargs[2], &tdev, &parent, &child)) == 0) {
		printf("symlink: failed: target path does not exist\n");
	} else if (sdev != tdev) {
		printf("symlink: failed: both paths must be on the same device\n");
	} else if (tmip = iget(tdev, tino), tmip == NULL) {
		printf("symlink: failed: target inode not found\n");
	} else if ((tmip->inode.i_mode & 0xF000) != 0x4000) {
		printf("symlink: failed: '%s' is not a directory\n", parent);
	} else if (search(tmip, child) != 0) {
		printf("symlink: failed: '%s' already exists\n", child);
	} else {	
		int inum = ialloc(tmip->dev);
		int bnum = balloc(tmip->dev);	

		help_mknod(tmip->dev, inum, bnum, 0xA1A4, 0, 1);
		char buf[BLOCK_SIZE];
		memset(buf, 0, BLOCK_SIZE);
		put_block(tmip->dev, bnum, buf);
		insert_entry(tmip, inum, child);
			
		tmip->inode.i_atime = time(0);
		tmip->dirty = 1;
		
		MINODE *mip = iget(tmip->dev, inum);
		strncpy((char *)mip->inode.i_block, myargs[1], 60);
		mip->dirty = 1;
		iput(mip);
	}
	
	if (smip)
		iput(smip);
	if (tmip)
		iput(tmip);
	if (parent)
		free(parent);
	if (child)
		free(child);
}

/* READLINK COMMAND*/

void my_readlink()
{
	int dev, ino;
	MINODE *mip = NULL;

	if (myargc == 1) {
		printf("readlink: missing operand\n");
	} else if ((ino = getino(&dev, myargs[1])) == 0) {
		printf("readlink: failed: path does not exist\n");
	} else if (mip = iget(dev, ino), mip == NULL) {
		printf("readlink: error: inode not found\n");
	} else if ((mip->inode.i_mode & 0xF000) != 0xA000) {
		printf("readlink: failed: '%s' not a symlink\n", myargs[1]);
	} else {
		printf("%s -> %s\n", myargs[1], (char *)(mip->inode.i_block));		
	}

	if (mip)
		iput(mip);
}

/* UNLINK COMMAND */

int file_unlink(char *path)
{
	int r = -1;
	int dev, pino, cino;
	char *parent = NULL, *child = NULL;
	MINODE *pmip = NULL, *cmip = NULL;

	if ((pino = parseargs(path, &dev, &parent, &child)) == 0) {
		printf("unlink: failed: path does not exist\n");
	} else if (pmip = iget(dev, pino), pmip == NULL) {
		printf("unlink: error: inode '%d' not found\n", pino);
	} else if ((pmip->inode.i_mode & 0xF000) != 0x4000) {
		printf("unlink: failed: '%s' is not a directory\n", parent);
	} else if ((cino = search(pmip, child)) == 0) {
		printf("unlink: failed: no such file '%s'\n", child);
	} else if (cmip = iget(dev, cino), cmip == NULL) {
		printf("unlink: error: inode '%d' not found\n", cino);
	} else if ((cmip->inode.i_mode & 0xF000) == 0x4000) {
		printf("unlink: failed: '%s' is not a file\n", child);
	} else if (!has_perm(cmip, 2)) {
		printf("unlink: failed: wrong permissions\n");
	} else {
		remove_entry(pmip, child);
		cmip->inode.i_links_count--;
		
		if (cmip->inode.i_links_count < 1) {
			clear_blocks(cmip);
			idealloc(cmip->dev, cmip->ino);	
		}

		pmip->dirty = 1;
		cmip->dirty = 1;

		r = 0;
	}

	if (pmip)
		iput(pmip);
	if (cmip)
		iput(cmip);
	if (parent)
		free(parent);
	if (child)
		free(child);

	return r;
}

void my_unlink()
{
	if (myargc == 1) {
		printf("unlink: missing operand\n");
	} else {
		file_unlink(myargs[1]);
	}
}

/* STAT COMMAND */

void my_stat()
{
	int dev, ino;
	MINODE *mip = NULL;

	if (myargc == 1) {
		printf("stat: missing operand\n");
	} else if ((ino = getino(&dev, myargs[1])) == 0) {
		printf("stat: failed: path does not exist\n");
	} else if (mip = iget(dev, ino), mip == NULL) {
		printf("stat: error: inode not found\n");
	} else {
		INODE *ip = &(mip->inode);

		char *name = basename(myargs[1]);
		printf("  File: '%s'\n", name);
		printf("Device: %d    ", dev);
		printf(" Inode: %d\n", ino);
		printf("  Size: %d    ", ip->i_size);
		printf("Blocks: %d    ", ip->i_blocks);
		printf(" Links: %d\n", ip->i_links_count);
		printf("  Mode: 0%o   ", ip->i_mode);
		printf("   Uid: %d    ", ip->i_uid);
		printf("   Gid: %d\n", ip->i_gid);
		
		char buffer[20];
		time_t inode_time;
		struct tm *timeinfo;

		inode_time = ip->i_atime;
		timeinfo = localtime(&inode_time);
		strftime(buffer, 20, "%F %H:%M:%S", timeinfo);
		printf("Access: %s\n", buffer);
		
		inode_time = ip->i_mtime;
		timeinfo = localtime(&inode_time);
		strftime(buffer, 20, "%F %H:%M:%S", timeinfo);
		printf("Modify: %s\n", buffer);
		
		inode_time = ip->i_ctime;
		timeinfo = localtime(&inode_time);
		strftime(buffer, 20, "%F %H:%M:%S", timeinfo);
		printf("Change: %s\n", buffer);
	}

	if (mip)
		iput(mip);
}

/* TOUCH COMMAND */

void my_touch()
{
	int dev, ino;
	MINODE *mip = NULL;

	if (myargc == 1) {
		printf("touch: missing operand\n");
	} else if ((ino = getino(&dev, myargs[1])) == 0) {
		file_creat(myargs[1]);
	} else if (mip = iget(dev, ino), mip == NULL) {
		printf("touch: error: inode not found\n");
	} else {
		time_t timer = time(0);
		mip->inode.i_atime = timer;
		mip->inode.i_mtime = timer;
		
		mip->dirty = 1;
	}

	if (mip)
		iput(mip);
}

/* CHMOD COMMAND */

void my_chmod()
{
	int dev, ino, mode;
	MINODE *mip = NULL;

	if (myargc < 3) {
		printf("chmod: missing operand");
	} else if (sscanf(myargs[1], "%o", &mode) < 1) {
		printf("chmod: failed: invalid input\n");
	} else if (mode > 0777) {
		printf("chmod: failed: invalid permission input\n");
	} else if ((ino = getino(&dev, myargs[2])) == 0) {
		printf("chmod: failed: path does not exist\n");
	} else if (mip = iget(dev, ino), mip == NULL) {
		printf("chmod: error: inode not found\n");
	} else {
		mip->inode.i_mode &= 0xF000;
		mip->inode.i_mode += mode;
		
		mip->dirty = 1;
	}

	if (mip)
		iput(mip);
}

/* CHOWN COMMAND */

void my_chown()
{
	int dev, ino, own;
	MINODE *mip = NULL;

	if (myargc < 3) {
		printf("chown: missing operand");
	} else if (sscanf(myargs[1], "%d", &own) < 1) {
		printf("chown: failed: invalid input\n");
	} else if ((ino = getino(&dev, myargs[2])) == 0) {
		printf("chown: failed: path does not exist\n");
	} else if (mip = iget(dev, ino), mip == NULL) {
		printf("chown: error: inode not found\n");
	} else {
		mip->inode.i_uid = own;
		
		mip->dirty = 1;
	}

	if (mip)
		iput(mip);
}

/* CHGRP COMMAND */

void my_chgrp()
{
	int dev, ino, grp;
	MINODE *mip = NULL;

	if (myargc < 3) {
		printf("chgrp: missing operand");
	} else if (sscanf(myargs[1], "%d", &grp) < 1) {
		printf("chgrp: failed: invalid input\n");
	} else if ((ino = getino(&dev, myargs[2])) == 0) {
		printf("chgrp: failed: path does not exist\n");
	} else if (mip = iget(dev, ino), mip == NULL) {
		printf("chgrp: error: inode not found\n");
	} else {
		mip->inode.i_gid = grp;
		
		mip->dirty = 1;
	}

	if (mip)
		iput(mip);
}

