#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

#include "project.h"

static void my_quit(int argc, char *argv[])
{
	printf("Cleaning up before closing filesystem... ");
	for (int i = 0; i < NMINODES; i++) {
		if (m_inodes[i].ref_count > 0) {
			// set refs to 1 so iput will save
			m_inodes[i].ref_count = 1;
			iput(&m_inodes[i]);
		}
	}
	printf("Done!\n");
}

/* LEVEL 1 COMMANDS */

static void my_ls(int argc, char *argv[])
{
	if (argc < 2) {
		shell_ls(".");
	} else {
		shell_ls(argv[1]);
	}
}

static void my_cd(int argc, char *argv[])
{
	if (argc < 2) {
		shell_cd(NULL);
	} else {
		shell_cd(argv[1]);
	}
}

static void my_pwd(int argc, char *argv[])
{
	shell_pwd();
}

static void my_mkdir(int argc, char *argv[])
{
	if (argc < 2) {
		printf("mkdir: missing operand\n");
	} else {
		file_mkdir(argv[1]);
	}
}

static void my_creat(int argc, char *argv[])
{
	if (argc < 2) {
		printf("creat: missing operand\n");
	} else {
		file_creat(argv[1]);
	}
}

static void my_rmdir(int argc, char *argv[])
{
	if (argc < 2) {
		printf("rmdir: missing operand\n");
	} else {
		file_rmdir(argv[1]);
	}
}

static void my_rm(int argc, char *argv[])
{
	if (argc < 2) {
		printf("rm: missing operand\n");
	} else {
		file_unlink(argv[1]);
	}
}

static void my_link(int argc, char *argv[])
{
	if (argc < 3) {
		printf("link: missing operand\n");
	} else {
		file_link(argv[1], argv[2]);
	}
}

static void my_symlink(int argc, char *argv[])
{
	if (argc < 3) {
		printf("symlink: missing operand\n");
	} else {
		file_symlink(argv[1], argv[2]);
	}
}

static void my_unlink(int argc, char *argv[])
{
	if (argc < 2) {
		printf("unlink: missing operand\n");
	} else {
		file_unlink(argv[1]);
	}
}

static void my_readlink(int argc, char *argv[])
{
	if (argc < 2) {
		printf("readlink: missing operand\n");
	} else {
		shell_readlink(argv[1]);
	}
}

static void my_stat(int argc, char *argv[])
{
	if (argc < 2) {
		printf("stat: missing operand\n");
	} else {
		shell_stat(argv[1]);
	}
}

static void my_touch(int argc, char *argv[])
{
	if (argc < 2) {
		printf("touch: missing operand\n");
	} else {
		file_touch(argv[1]);
	}
}

static void my_chmod(int argc, char *argv[])
{
	int mode;
	if (argc < 3) {
		printf("chmod: missing operand");
	} else if (sscanf(argv[1], "%o", &mode) < 1) {
		printf("chmod: failed: invalid input\n");
	} else {
		file_chmod(mode, argv[2]);
	}
}


static void my_chown(int argc, char *argv[])
{
	int own;
	if (argc < 3) {
		printf("chown: missing operand");
	} else if (sscanf(argv[1], "%d", &own) < 1) {
		printf("chown: failed: invalid input\n");
	} else {
		file_chown(own, argv[2]);
	}
}

static void my_chgrp(int argc, char *argv[])
{
	int grp;
	if (argc < 3) {
		printf("chgrp: missing operand");
	} else if (sscanf(argv[1], "%d", &grp) < 1) {
		printf("chgrp: failed: invalid input\n");
	} else {
		file_chgrp(grp, argv[2]);
	}
}

/* LEVEL 2 COMMANDS */

static void my_open(int argc, char *argv[])
{
	int mode;
	if (argc < 3) {
		printf("open: missing operand\n");
	} else if (sscanf(argv[2], "%u", &mode) < 1) {
		printf("open: failed: invalid input\n");
	} else {
		int fd = file_open(argv[1], mode);
		if (fd >= 0)
			printf("file descriptor: %d\n", fd);
	}
}

static void my_close(int argc, char *argv[])
{
	int fd;
	if (argc < 2) {
		printf("close: missing operand\n");
	} else if (sscanf(argv[1], "%u", &fd) < 1) {
		printf("close: failed: invalid input\n");
	} else {
		int r = file_close(fd);
		if (r == 0)
			printf("file closed\n");
	}
}

static void my_lseek(int argc, char *argv[])
{
	int fd, pos;
	if (argc < 3) {
		printf("lseek: missing operand\n");
	} else if (sscanf(argv[1], "%u", &fd) < 1) {
		printf("lseek: failed: invalid fd\n");
	} else if (sscanf(argv[2], "%u", &pos) < 1) {
		printf("lseek: failed: invalid offset\n");
	} else {
		int off = file_lseek(fd, pos);
		if (off >= 0)
			printf("previous position: %d\n", off);
	}
}

static void my_pfd(int argc, char *argv[])
{
	shell_pfd();
}

static void my_read(int argc, char *argv[])
{
	int fd, bytes;
	if (argc < 3) {
		printf("read: missing operand\n");
	} else if (sscanf(argv[1], "%u", &fd) < 1) {
		printf("read: failed: invalid fd\n");
	} else if (sscanf(argv[2], "%u", &bytes) < 1) {
		printf("read: failed: invalid bytes\n");
	} else {
		char buf[bytes];
		int n = file_read(fd, buf, bytes);
		printf("%s\n", buf);
		printf("read %d bytes\n", n);
	}
}

static void my_write(int argc, char *argv[])
{
	int fd;
	if (argc < 3) {
		printf("write: missing operand\n");
	} else if (sscanf(argv[1], "%u", &fd) < 1) {
		printf("write: failed: invalid fd\n");
	} else {
		int len = strlen(argv[2]);

		char buf[len+1];
		strncpy(buf, argv[2], len);
		buf[len] = 0;

		int n = file_write(fd, buf, len);
		printf("wrote %d bytes\n", n);
	}
}

static void my_cat(int argc, char *argv[])
{
	if (argc < 2) {
		printf("cat: missing operand\n");
	} else {
		shell_cat(argv[1]);
	}
}

static void my_cp(int argc, char *argv[])
{
	if (argc < 3) {
		printf("cp: missing operand\n");
	} else {
		file_cp(argv[1], argv[2]);
	}
}

static void my_mv(int argc, char *argv[])
{
	if (argc < 3) {
		printf("mv: missing operand\n");
	} else {
		file_mv(argv[1], argv[2]);
	}
}

/* LEVEL 3 COMMANDS */

static void my_mount(int argc, char *argv[])
{
	if (argc < 2) {
		print_mounttab();
	} else if (argc < 3) {
		printf("mount: missing operand\n");
	} else {
		mount(argv[1], argv[2]);
	}
}

static void my_umount(int argc, char *argv[])
{
	if (argc < 2) {
		printf("umount: missing operand\n");
	} else {
		umount(argv[1]);
	}
}

static void my_switch(int argc, char *argv[])
{
	int uid;
	if (argc < 2) {
		printf("switch: missing operand\n");
	} else if (sscanf(argv[1], "%u", &uid) < 1) {
		printf("switch: invalid input\n");
	} else {
		pswitch(uid);
	}
}

/* COMMAND EXECUTION */

static struct {
	char *name;
	void (*func)(int, char *[]);
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
 * @input: The line of input from the commandline.
 *
 * Executes the input from the commandline.
 *
 * Returns: 0 if the program should quit, 1 otherwise.
 */
int execute(char *input)
{
	if (!*input)
		return 1;

	char *argv[64];
	int argc = tokenize(input, " ", argv);

	int len = sizeof(commands) / sizeof(commands[0]);
	for (int i = 0; i < len; i++) {
		if (strcmp(argv[0], commands[i].name) == 0) {
			commands[i].func(argc, argv);
			return (i != 0);
		}
	}
	printf("%s: command not found\n", argv[0]);
	return 1;
}

