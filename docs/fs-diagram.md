# File System Organization

> 360 Week 3 Notes


## File system Data Structure Diagram

KEEP A COPY OF THIS DIAGRAM FOR REFERENCE when designing your algorithms.
```
1. Running
                                                        ||*********************
     |                                                  ||
     V        |---- PointerToCWD ------|                ||
              |                        |                || 
2  PROC[ ]    |    3. OFT[ ]           V 4.MINODE[ ]    ||         Disk dev
 ===========  |    ===========          ============    ||   ==================
 |nextProcPtr |    |OpenMode |          |  INODE   |    ||   |         INODE   
 |pid, ppid   |    |refCount |          | -------  |    ||   ================== 
 |uid         |    |MinodePtr|          | dev,ino  |    || 
 |cwd --------|    |offset   |          | refCount |    ||*********************
 |                 ===========          | dirty    |
 |fd[10]                                | mounted  |
 | ------                     <---------| mTablePtr|
 | ------                     |         |==========|
 | ------                     |         |  INODE   |
 | ------                     |         | -------  |
 ===========                  |         |  dev,ino |
                              |         |  refCount|
                              |         |  dirty   |
                              |         |  mounted |
                              |         |  mtPtr   |
                              |         |==========|
            PointAtRootInode  |                   
                   ^          |                   
                   |          V 
                   |      5.   MountTable[ ]
                   |  ------- 0 ------------- 1 -----
                   |  |   devNumber    |            |
                   |--|   MinodePtr    |            |
                      -------------------------------
                      |   deviceName   |            |
                      ------ Optional entries -------
                      | nblocks,ninodes|            |
                      | bmap,imap,iblk |            |
                      -------------------------------
```

This diagram shows the data structures of the file system in an OS KERNEL

1. is a PROC pointer pointing at the PROC structure of the current running process. Each process has a Current Working Directory, cwd, which is initialized to point at the in-memory root inode.

2. is the PROC structure of processes. Every file operation is performed by the current ruuning process.

3. is the Open File Table (OFT). Each OFT entry represents an instance of an opened file. We shall discuss OFT later.

4. is the in-memory inodes array, MINODE[NMINODES]. Each minode entry contains a sub-structure INODE, which is the INODE structure on disk.  Whenever a file is referenced, its inode must be brought into memory. In order to ensure ONLY ONE copy of every inode in in memory, a needed inode will be loaded into a MINODE slot. The (dev, ino) fields identify where the inode came from (for writing back to disk). The refCount represents how many processes are using this minode. The dirty field says whether the INODE has been modified or not. If an minode is dirty, the last user of the minode must write the INODE back to disk. The mounted flag says whether this DIR has been mounted on or not. If mounted on, the mTablePtr points at the MountTable entry.

5. is the MountTable (MT). Each entry represetns a device that has been 
   mounted (on a DIR).  When a file system starts, it must mount a device on
   the Root DIR /. That device is called the root device. So the first thing
   a file system must do is to mount-root. As shown, MT[0] represents the
   mounted root device. The dev field identifies which device this is. The 
   MinodePtr points at the DIR that's mounted on. (in 4, the DIR also points 
   back to the mounted device). Knowing the dev, we can always access the 
   device to get its Superblock, bitmaps, rootInode, etc. For convenience, some
   oftenly used information are kept in the MountTable for quick reference, e.g.
   nblocks, ninodes. Here is an example of their usage: If process wants to 
   release an inode, it calls idealloc(ino) to deallocate the inumber of the 
   inode. The caller may pass in an ino > actual number of inodes (hence 
   exceeds the range of the inodes bitmap). We can check the ino against 
   ninodes in the MT table to avoid tunning the worng bits on in the bitmap. 
   Similarly for nblocks. 


   The posted type.h file contains data structure types of our FS system. You 
   may include type.h in your C program.

   When a file system starts up, it must initialize itself first, by

       fs_init(){ ..............};

Then, it must "mount a device" containing a file system as the "root device".
Mounting a root device amounts to loading the root INODE on that device into
memory and establish it as the root directory /.

In a file system, every operation is done by the current running "process". 
Each process is represented by a PROC structure:

     struct proc{
        int pid;
        int uid;       // uid = 0 for SUPERUSER; non-zero for ordinary user
        MINODE *cwd;   // CWD pointer -> CWD INODE in memory
        //other fields later;
     } proc[NPROC];

