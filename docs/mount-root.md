<Title>360 Assignment</Title>
<Body bgcolor="#00CCCC" text="#000000">

<H1>LAB Assignment #7</H1>

<Pre>

               DUE : Week of April 3, 2016
                       REQUIREMENTS: 

1. Level-1 Data Structures

PROC* running           MINODE *root                          
      |                          |              
      |                          |               ||*********************
      V                          |  MINODE       || 
    PROC[0]                      V minode[100]   ||         Disk dev
 =============  |-pointerToCWD-> ============    ||   ==================
 |nextProcPtr|  |                |  INODE   |    ||   |     INODEs   
 |pid = 1    |  |                | -------  |    ||   ================== 
 |uid = 0    |  |                | (dev,2)  |    || 
 |cwd --------->|                | refCount |    ||*********************
 |           |                   | dirty    |
 |fd[10]     |                   |          |         
 | ------    |       
 | - ALL 0 - |                   |==========|         
 | ------    |                   |  INODE   |          
 | ------    |                   | -------  |   
 =============                   | (dev,ino)|   
                                 |  refCount|  
   PROC[1]          ^            |  dirty   |   
    pid=2           |
    uid=1           |  
    cwd ----> root minode        |==========|  


DOWNLOAD samples/mydisk, which is an (empty) EXT2 FS or use your own virtual FD.

2. init() // Initialize data structures of LEVEL-1:
   {
     (1). 2 PROCs, P0 with uid=0, P1 with uid=1, all PROC.cwd = 0
     (2). MINODE minode[100]; all with refCount=0
     (3). MINODE *root = 0;
   }

3.. Write C code for
         int ino     = getino(int *dev, char *pathname)
         MINODE *mip = iget(dev, ino)

                       iput(MINDOE *mip)

4. mount_root()  // mount root file system, establish / and CWDs
   {
      open device for RW
      read SUPER block to verify it's an EXT2 FS
   
      root = iget(dev, 2);    /* get root inode */
    
      Let cwd of both P0 and P1 point at the root minode (refCount=3)
          P0.cwd = iget(dev, 2); 
          P1.cwd = iget(dev, 2);
    }


5. ls [pathname] command:
   {
      int ino, dev = running->cwd->dev;
      MINODE *mip = running->cwd;

      if (pathname){   // ls pathname:
          if (pathname[0]=='/')
             dev = root->dev;
          ino         = getino(&dev, pathname);
          MINODE *mip = iget(dev, ino);
      }
      // mip points at minode; 
      // Each data block of mip->INODE contains DIR entries
      // print the name strings of the DIR entries
   }
      
6. cd(char *pathname)
   {
      if (no pathname)
         cd to root;
      else
         cd to pathname;
   }

7. int ialloc(dev): allocate a free inode (number)
   int balloc(dev): allocate a free block (number)

   NOTE: disk blocks and inodes COUNT FROM 1, not 0.
         In both bitmaps, bit0 = item1, bit1=iten2, etc.

8.int main()
  {
     init();
     mount_root();
     // ask for a command string, e.g. ls pathname
     ls(pathname);
     // ask for a command string, e.g. cd pathname
     cd(pathname);
     // ask for a command string, e.g. stat pathname
     stat(pathname, &mystat); // struct stat mystat; print mystat information
  }       

9.int quit()
  {
      iput all DIRTY minodes before shutdown
  }


/****************** FINAL FORM OF main() **************************/

int main(int argc, char *argv[ ]) 
{
  int i, cmd; 
  char line[128], cname[64];

  init();

  while(1){
      printf("P%d running: ", running->pid);
      printf("input command : ");
      fgets(line, 128, stdin);
      line[strlen(line)-1] = 0;  // kill the \r char at end
      if (line[0]==0) continue;

      sscanf(line, "%s %s %64c", cname, pathname, parameter);

      cmd = findCmd(cname); // map cname to an index

      switch(cmd){
           ------------ LEVEL 1 -------------------
           case 0 : menu();                   break;
           case 1 : make_dir();               break;
           case 2 : change_dir();             break;
           case 3 : pwd(cwd);                 break;
           case 4 : list_dir();               break;
           case 5 : rmdir();                  break;
           case 6 : creat_file();             break;
           case 7 : link();                   break;
           case 8 : unlink();                 break;
           case 9 : symlink();                break;
           case 10: rm_file();                break;
           case 11: chmod_file();             break;
           case 12: chown_file();             break;
           case 13: stat_file();              break;
           case 14: touch_file();             break;

           -------------- LEVEL 2 ------------------
           case 20: open_file();              break;
           case 21: close_file();             break;
           case 22: pfd();                    break;
           case 23: lseek_file();             break;
           case 24: access_file();            break;
           case 25: read_file();              break;
           case 26: write_file();             break;
           case 27: cat_file();               break;
           case 28: cp_file();                break;
           case 29: mv_file();                break;

           ------------- LEVEL 3 -------------------
           case 30: mount();                  break;
           case 31: umount();                 break;
           case 32: cs();                     break;
           case 33: do_fork();                break;
           case 34: do_ps();                  break;
          
           case 40: sync();                   break; 
           case 41: quit();                   break; 
           default: printf("invalid command\n");
                    break;
      }
  }
} /* end main */
***************************************************************************

 INSTEAD OF A switch-case table, you MUST use a Table of Function Pointers







