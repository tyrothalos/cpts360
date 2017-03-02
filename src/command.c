#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

#include "command.h"
#include "global.h"
#include "level1.h"
#include "level2.h"
#include "level3.h"
#include "type.h"
#include "util.h"

void my_quit()
{
	printf("Cleaning up before closing filesystem...\n");
	int i;
	for (i = 0; i < NMINODES; i++) {
		if (m_inodes[i].ref_count > 0) {
			// set refs to 1 so iput will save
			m_inodes[i].ref_count = 1;
			iput(&m_inodes[i]);
		}
	}
	printf("Finished cleaning up!\n");
}

/* LEVEL 1 COMMANDS */

/* LEVEL 2 COMMANDS */


void my_open()
{
	int mode;
	if (myargc < 3) {
		printf("open: missing operand\n");
	} else if (sscanf(myargs[2], "%u", &mode) < 1) {
		printf("open: failed: invalid input\n");
	} else if (mode < 0 || mode > 3) {
		printf("open: failed: invalid mode input\n");
	} else {
		int fd = file_open(myargs[1], mode);
		if (fd >= 0)
			printf("file descriptor: %d\n", fd);
	}
}

void my_close()
{
	int fd;
	if (myargc == 1) {
		printf("close: missing operand\n");
	} else if (sscanf(myargs[1], "%u", &fd) < 1) {
		printf("close: failed: invalid input\n");
	} else {
		int r = file_close(fd);
		if (r == 0)
			printf("file closed\n");
	}
}

void my_lseek()
{
	int fd, pos;
	if (myargc < 3) {
		printf("lseek: missing operand\n");
	} else if (sscanf(myargs[1], "%u", &fd) < 1) {
		printf("lseek: failed: invalid fd\n");
	} else if (sscanf(myargs[2], "%u", &pos) < 1) {
		printf("lseek: failed: invalid offset\n");
	} else {
		int off = file_lseek(fd, pos);
		if (off >= 0)
			printf("previous position: %d\n", off);
	}
}

void my_pfd()
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

void my_read()
{
	int fd, bytes;
	if (myargc < 3) {
		printf("read: missing operand\n");
	} else if (sscanf(myargs[1], "%u", &fd) < 1) {
		printf("lseek: failed: invalid fd\n");
	} else if (sscanf(myargs[2], "%u", &bytes) < 1) {
		printf("lseek: failed: invalid bytes\n");
	} else {
		char buf[bytes];
		int n = file_read(fd, buf, bytes);
		printf("%s\n", buf);
		printf("read %d bytes\n", n);
	}
}

void my_write()
{
	int fd;
	if (myargc < 3) {
		printf("read: missing operand\n");
	} else if (sscanf(myargs[1], "%u", &fd) < 1) {
		printf("read: failed: invalid fd\n");
	} else {
		int len = strlen(myargs[2]);
		
		char buf[len+1];
		strncpy(buf, myargs[2], len);
		buf[len] = 0;
		
		int n = file_write(fd, buf, len);
		printf("wrote %d bytes\n", n);
	}
}

/* CAT COMMAND */

void my_cat()
{
	int fd = -1;
	if (myargc == 1) {
		printf("cat: missing operand\n");
	} else if ((fd = file_open(myargs[1], 0)) < 0) {
		printf("cat: failed: could not open file\n");
	} else {
		char buf[BLOCK_SIZE+1];
		buf[BLOCK_SIZE] = 0;

		int i, n;
		while ((n = file_read(fd, buf, BLOCK_SIZE)) > 0) {
			buf[n] = 0;
			for (i = 0; i < n; i++) {
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

void my_cp()
{
	if (myargc < 3) {
		printf("cp: missing operand\n");
	} else {
		file_cp(myargs[1], myargs[2]);
	}
}

void my_mv()
{
	if (myargc < 3) {
		printf("mv: missing operand\n");
	} else {
		file_mv(myargs[1], myargs[2]);
	}
}

/* LEVEL 3 COMMANDS */

void my_mount()
{
	if (myargc == 1) {
		print_mounttab();
	} else if (myargc < 3) {
		printf("mount: missing operand\n");
	} else {
		mount(myargs[1], myargs[2]);
	}
}

void my_umount()
{
	if (myargc == 1) {
		printf("umount: missing operand\n");
	} else {
		umount(myargs[1]);
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

/* COMMAND EXECUTION */

static struct {
	char *name;
	void (*func)();
} commands[] = {
	{ "quit"    , my_quit     },
	{ "ls"      , my_ls       },
	{ "cd"      , my_cd       },
	{ "pwd"     , my_pwd      },
	{ "mkdir"   , my_mkdir    },
	{ "rmdir"   , my_rmdir    },
	{ "creat"   , my_creat    },
	{ "rm"      , my_rm       },
	{ "link"    , my_link     },
	{ "symlink" , my_symlink  },
	{ "readlink", my_readlink },
	{ "unlink"  , my_unlink   },
	{ "stat"    , my_stat     },
	{ "touch"   , my_touch    },
	{ "chmod"   , my_chmod    },
	{ "chown"   , my_chown    },
	{ "chgrp"   , my_chgrp    },
	{ "open"    , my_open     },
	{ "close"   , my_close    },
	{ "lseek"   , my_lseek    },
	{ "pfd"     , my_pfd      },
	{ "read"    , my_read     },
	{ "write"   , my_write    },
	{ "cat"     , my_cat      },
	{ "cp"      , my_cp       },
	{ "mv"      , my_mv       },
	{ "mount"   , my_mount    },
	{ "umount"  , my_umount   },
	{ "switch"  , my_switch   }
};

/*
 * Returns whether or not the program
 * should stop running (1 for yes, 0 for no).
 */
int execute(char *cmd)
{
	int len = sizeof(commands) / sizeof(commands[0]);
	for (int i = 0; i < len; i++) {
		if (strcmp(cmd, commands[i].name) == 0) {
			commands[i].func();
			return (i == 0);
		}
	}
	printf("%s: command not found\n", cmd);
	return 0;
}

