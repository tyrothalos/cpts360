#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

#include "project.h"

static void my_quit()
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

static void my_ls()
{
	if (myargc < 2) {
		shell_ls(".");
	} else {
		shell_ls(myargs[1]);
	}
}

static void my_cd()
{
	if (myargc < 2) {
		shell_cd(NULL);
	} else {
		shell_cd(myargs[1]);
	}
}

static void my_pwd()
{
	shell_pwd();
}

static void my_mkdir()
{
	if (myargc < 2) {
		printf("mkdir: missing operand\n");
	} else {
		file_mkdir(myargs[1]);
	}
}

static void my_creat()
{
	if (myargc < 2) {
		printf("creat: missing operand\n");
	} else {
		file_creat(myargs[1]);
	}
}

static void my_rmdir()
{
	if (myargc < 2) {
		printf("rmdir: missing operand\n");
	} else {
		file_rmdir(myargs[1]);
	}
}

static void my_rm()
{
	if (myargc < 2) {
		printf("rm: missing operand\n");
	} else {
		file_unlink(myargs[1]);
	}
}

static void my_link()
{
	if (myargc < 3) {
		printf("link: missing operand\n");
	} else {
		file_link(myargs[1], myargs[2]);
	}
}

static void my_symlink()
{
	if (myargc < 3) {
		printf("symlink: missing operand\n");
	} else {
		file_symlink(myargs[1], myargs[2]);
	}
}

static void my_unlink()
{
	if (myargc < 2) {
		printf("unlink: missing operand\n");
	} else {
		file_unlink(myargs[1]);
	}
}

static void my_readlink()
{
	if (myargc < 2) {
		printf("readlink: missing operand\n");
	} else {
		shell_readlink(myargs[1]);
	}
}

static void my_stat()
{
	if (myargc < 2) {
		printf("stat: missing operand\n");
	} else {
		shell_stat(myargs[1]);
	}
}

static void my_touch()
{
	if (myargc < 2) {
		printf("touch: missing operand\n");
	} else {
		file_touch(myargs[1]);
	}
}

static void my_chmod()
{
	int mode;
	if (myargc < 3) {
		printf("chmod: missing operand");
	} else if (sscanf(myargs[1], "%o", &mode) < 1) {
		printf("chmod: failed: invalid input\n");
	} else if (mode > 0777) {
		printf("chmod: failed: invalid permission input\n");
	} else {
		file_chmod(mode, myargs[2]);
	}
}


static void my_chown()
{
	int own;
	if (myargc < 3) {
		printf("chown: missing operand");
	} else if (sscanf(myargs[1], "%d", &own) < 1) {
		printf("chown: failed: invalid input\n");
	} else {
		file_chown(own, myargs[2]);
	}
}

static void my_chgrp()
{
	int grp;
	if (myargc < 3) {
		printf("chgrp: missing operand");
	} else if (sscanf(myargs[1], "%d", &grp) < 1) {
		printf("chgrp: failed: invalid input\n");
	} else {
		file_chgrp(grp, myargs[2]);
	}
}

/* LEVEL 2 COMMANDS */

static void my_open()
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

static void my_close()
{
	int fd;
	if (myargc < 2) {
		printf("close: missing operand\n");
	} else if (sscanf(myargs[1], "%u", &fd) < 1) {
		printf("close: failed: invalid input\n");
	} else {
		int r = file_close(fd);
		if (r == 0)
			printf("file closed\n");
	}
}

static void my_lseek()
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

static void my_pfd()
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

static void my_read()
{
	int fd, bytes;
	if (myargc < 3) {
		printf("read: missing operand\n");
	} else if (sscanf(myargs[1], "%u", &fd) < 1) {
		printf("read: failed: invalid fd\n");
	} else if (sscanf(myargs[2], "%u", &bytes) < 1) {
		printf("read: failed: invalid bytes\n");
	} else {
		char buf[bytes];
		int n = file_read(fd, buf, bytes);
		printf("%s\n", buf);
		printf("read %d bytes\n", n);
	}
}

static void my_write()
{
	int fd;
	if (myargc < 3) {
		printf("write: missing operand\n");
	} else if (sscanf(myargs[1], "%u", &fd) < 1) {
		printf("write: failed: invalid fd\n");
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

static void my_cat()
{
	int fd = -1;
	if (myargc < 2) {
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

static void my_cp()
{
	if (myargc < 3) {
		printf("cp: missing operand\n");
	} else {
		file_cp(myargs[1], myargs[2]);
	}
}

static void my_mv()
{
	if (myargc < 3) {
		printf("mv: missing operand\n");
	} else {
		file_mv(myargs[1], myargs[2]);
	}
}

/* LEVEL 3 COMMANDS */

static void my_mount()
{
	if (myargc < 2) {
		print_mounttab();
	} else if (myargc < 3) {
		printf("mount: missing operand\n");
	} else {
		mount(myargs[1], myargs[2]);
	}
}

static void my_umount()
{
	if (myargc < 2) {
		printf("umount: missing operand\n");
	} else {
		umount(myargs[1]);
	}
}

static void my_switch()
{
	int uid;
	if (myargc < 2) {
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
 * execute:
 * @cmd: The name of the command to execute.
 *
 * Executes the command with the given name, if it exists.
 *
 * Returns: 1 if the program should quit, 0 otherwise.
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

