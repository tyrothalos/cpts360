#ifndef PROJECT_TYPE_H
#define PROJECT_TYPE_H

#include <ext2fs/ext2_fs.h>

/* TYPE ALIASES */
typedef struct ext2_super_block SUPER;
typedef struct ext2_group_desc  GROUP;
typedef struct ext2_inode       INODE;
typedef struct ext2_dir_entry_2 DIR;    // need this for new version of e2fs

#define BLOCK_SIZE 1024
#define INODE_SIZE  128

// Block number of EXT2 FS on FD
#define SUPER_BLOCK 1
#define GROUP_BLOCK 2
#define ROOT_INODE  2

// Default dir and regular file modes
#define DIR_MODE    0040777
#define FILE_MODE   0100644
#define SUPER_MAGIC  0xEF53
#define SUPER_USER        0

// Proc status
#define FREE    0
#define READY   1
#define RUNNING 2

// Table sizes
#define NMINODES 100
#define NMOUNT    10
#define NPROC      2
#define NFD       10

// Open File Table
typedef struct oft {
	struct minode *inodeptr;
	int   mode;
	int   ref_count;
	int   offset;
} OFT;

// PROC structure
typedef struct proc {
	OFT   *fd[NFD];
	struct minode *cwd;
	int   uid;
	int   pid;
	int   gid;
	int   status;
} PROC;

// In-memory inodes structure
typedef struct minode {
	INODE inode;            // disk inode
	struct mount *mountptr;
	int   dev;
	int   ino;
	int   ref_count;
	int   dirty;
	int   mounted;
} MINODE;

// Mount Table structure
typedef struct mount {
	MINODE *mounted_inode;
	char   name[64];
	char   mount_name[64];
	int    dev;
	int    nblocks;
	int    ninodes;
	int    bmap;
	int    imap;
	int    iblk;
} MOUNT;

#endif

