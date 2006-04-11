// Description
// $Id: mkfs.ffs2.c,v 1.5 2005/11/07 11:15:12 gleixner Exp $
/* ######################################################################

   Microsoft Flash File System 2

   Information for the FFS2.0 was found in Microsoft's knowledge base,
   http://msdn.microsoft.com/isapi/msdnlib.idc?theURL=/library/specs/S346A.HTM
   Try searching for "Flash File System" if it has been moved

   This program creates an empty file system.

   ##################################################################### */

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <time.h>
#include <sys/ioctl.h>
#include <sys/mount.h>

#include <mtd/mtd-user.h"
#include <linux/ffs2_fs.h"

static unsigned long BlockSize = 128*1024;
static int Fd;

// Erase a single block
int EraseBlock(unsigned Number)
{
   unsigned char Blank[512];
   unsigned I;

   memset(Blank,0xFF,sizeof(Blank));

   if (lseek(Fd,BlockSize*Number,SEEK_SET) != BlockSize*Number)
      return -1;

   for (I = 0; I*sizeof(Blank) != BlockSize; I++)
   {
      if (write(Fd,Blank,sizeof(Blank)) != sizeof(Blank))
      return -1;
   }

   return 0;
}

int main (int argc,char * const argv[])
{
   struct mtd_info meminfo;
   unsigned Device;
   unsigned int Opt;
   unsigned I;
   unsigned long Length;
   unsigned long Spares = 1;
   unsigned long Start = 0;

   // Process options
   while ((Opt = getopt(argc,argv,"b:s:")) != EOF)
   {
      switch (Opt)
      {
	 case 'b':
	 BlockSize = strtol(optarg, NULL, 0);
	 break;

	 case 's':
	 Start = strtol(optarg, NULL, 0);
	 break;

	 case '?':
	 return 16;
      }
   }

   // Find the device name
   Device = optind;
   for (;Device < argc && (argv[Device][0] == 0 ||
			   argv[Device][0] == '-'); Device++);
   if (Device >= argc)
   {
      fprintf(stderr,"You must specify a device\n");
      return 16;
   }

   // Open and size the device
   if ((Fd = open(argv[Device],O_RDWR)) < 0)
   {
      fprintf(stderr,"File open error\n");
      return 8;
   }
   if (ioctl(Fd,BLKGETSIZE,&Length) < 0)
   {
      Length = lseek(Fd,0,SEEK_END);
      lseek(Fd,0,SEEK_SET);
   }
   else
      Length *= 512;

   if ((Start + 1)*BlockSize > Length)
   {
      fprintf(stderr,"The flash is not large enough\n");
   }

   printf("Total size is %lu, %lu byte erase "
	  "blocks for %lu blocks with %lu spares.\n",Length,BlockSize,
	  Length/BlockSize,Spares);
   if (Start != 0)
      printf("Skiping the first %lu bytes\n",Start*BlockSize);

   if (ioctl(Fd,MEMGETINFO,&meminfo) == 0)
   {
      struct erase_info erase;
      printf("Performing Flash Erase");
      fflush(stdout);

      erase.length = Length - Start*BlockSize;
      erase.start = Start*BlockSize;
      if (ioctl(Fd,MEMERASE,&erase) != 0)
      {
	 perror("\nMTD Erase failure");
	 close(Fd);
	 return 8;
      }
      printf(" done\n");
   }
   else
   {
      for (I = Start; I <= Length/BlockSize; I++)
      {
	 printf("Erase %u\r",I);
	 fflush(stdout);
	 if (EraseBlock(I) != 0)
	 {
	    perror(argv[Device]);
	    close(Fd);
	    return 8;
	 }
      }
   }

   for (I = 0; I != Length/BlockSize; I++)
   {
      struct ffs2_block block;

      // Write the block structure
      memset(&block,0xFF,sizeof(block));
      block.EraseCount = 1;
      block.BlockSeq = I;
      block.BlockSeqChecksum = 0xFFFF ^ block.BlockSeq;
      block.Status = (block.Status & (~FFS_STATE_MASK)) | FFS_STATE_READY;

      // Is Spare
      if (I >= Length/BlockSize - Spares)
      {
	 block.BlockSeq = 0xFFFF;
	 block.BlockSeqChecksum = 0xFFFF;
	 block.Status = (block.Status & (~FFS_STATE_MASK)) | FFS_STATE_SPARE;
      }

      // Setup the boot record and the root record
      if (I == 0)
      {
	 struct ffs2_bootrecord boot;
	 struct ffs2_blockalloc alloc[2];
	 unsigned char Tmp[300];
	 struct ffs2_entry *root = (struct ffs2_entry *)Tmp;

	 block.BootRecordPtr = 0;
	 block.Status = (block.Status & (~FFS_BOOTP_MASK)) | FFS_BOOTP_CURRENT;

	 boot.Signature = 0xF1A5;
	 boot.SerialNumber = time(0);
	 boot.FFSWriteVersion = 0x200;
	 boot.FFSReadVersion = 0x200;
	 boot.TotalBlockCount = Length/BlockSize;
	 boot.SpareBlockCount = Spares;
	 boot.BlockLen = BlockSize;
	 boot.RootDirectoryPtr = 0x1;
	 boot.BootCodeLen = 0;

	 memset(root,0xFF,sizeof(*root));
	 root->Status = (root->Status & (~FFS_ENTRY_TYPEMASK)) | FFS_ENTRY_TYPEDIR;
	 root->NameLen = strlen("root");
	 root->Time = (__u16)boot.SerialNumber;
	 root->Date = (__u16)(boot.SerialNumber >> 16);
	 root->VarStructLen = 0;
	 strcpy(root->Name,"root");

	 // Boot Block allocation structure
	 alloc[1].Status = (0xFF & (~FFS_ALLOC_SMASK)) | FFS_ALLOC_ALLOCATED;
	 alloc[1].Status &= 0xFF & (~FFS_ALLOC_EMASK);
	 alloc[1].Offset[0] = 0;
	 alloc[1].Offset[1] = 0;
	 alloc[1].Offset[2] = 0;
	 alloc[1].Len = FFS_SIZEOF_BOOT;

	 // Root Dir allocation structure
	 alloc[0].Status = (0xFF & (~FFS_ALLOC_SMASK)) | FFS_ALLOC_ALLOCATED;
	 alloc[0].Offset[0] = FFS_SIZEOF_BOOT;
	 alloc[0].Offset[1] = 0;
	 alloc[0].Offset[2] = 0;
	 alloc[0].Len = FFS_SIZEOF_ENTRY + root->NameLen;

	 // Write the two headers
	 if (lseek(Fd,BlockSize*(I+Start),SEEK_SET) < 0 ||
	     write(Fd,&boot,FFS_SIZEOF_BOOT) != FFS_SIZEOF_BOOT ||
	     write(Fd,root,FFS_SIZEOF_ENTRY + root->NameLen) != FFS_SIZEOF_ENTRY + root->NameLen)
	 {
	    perror("Failed writing headers");
	    close(Fd);
	    return 8;
	 }

	 // And the two allocation structures
	 if (lseek(Fd,BlockSize*(I+Start+1) - FFS_SIZEOF_BLOCK - sizeof(alloc),
		   SEEK_SET) <= 0 ||
	     write(Fd,alloc,sizeof(alloc)) != sizeof(alloc))
	 {
	    perror("Failed writing allocations");
	    close(Fd);
	    return 8;
	 }
      }

      if (lseek(Fd,BlockSize*(I+Start+1) - FFS_SIZEOF_BLOCK,
		SEEK_SET) <= 0 ||
	  write(Fd,&block,FFS_SIZEOF_BLOCK) != FFS_SIZEOF_BLOCK)
      {
	 perror("Failed writing block");
	 close(Fd);
	 return 8;
      }
   }
   printf("\n");

   return 0;
}
