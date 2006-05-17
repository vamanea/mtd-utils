/*
 * Build a JFFS image in a file, from a given directory tree.
 *
 * By default, builds an image that is of the same endianness as the
 * host.
 * The -a option can be used when building for a target system which
 * has a different endianness than the host.
 */

/* $Id: mkfs.jffs.c,v 1.15 2005/11/07 11:15:13 gleixner Exp $  */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#include <linux/types.h>
#include <stdint.h>
#include <mtd_swab.h>
#include <ctype.h>


#define BLOCK_SIZE 1024
#define JFFS_MAGIC 0x34383931 /* "1984" */
#define JFFS_MAX_NAME_LEN 256
#define JFFS_MIN_INO 1
#define JFFS_TRACE_INDENT 4
#define JFFS_ALIGN_SIZE 4
static int MAX_CHUNK_SIZE = 32768;

/* How many padding bytes should be inserted between two chunks of data
   on the flash?  */
#define JFFS_GET_PAD_BYTES(size) ((JFFS_ALIGN_SIZE                     \
				  - ((uint32_t)(size) % JFFS_ALIGN_SIZE)) \
				  % JFFS_ALIGN_SIZE)


struct jffs_raw_inode
{
  uint32_t magic;    /* A constant magic number.  */
  uint32_t ino;      /* Inode number.  */
  uint32_t pino;     /* Parent's inode number.  */
  uint32_t version;  /* Version number.  */
  uint32_t mode;     /* file_type, mode  */
  uint16_t uid;
  uint16_t gid;
  uint32_t atime;
  uint32_t mtime;
  uint32_t ctime;
  uint32_t offset;     /* Where to begin to write.  */
  uint32_t dsize;      /* Size of the file data.  */
  uint32_t rsize;      /* How much are going to be replaced?  */
  uint8_t nsize;       /* Name length.  */
  uint8_t nlink;       /* Number of links.  */
  uint8_t spare : 6;   /* For future use.  */
  uint8_t rename : 1;  /* Is this a special rename?  */
  uint8_t deleted : 1; /* Has this file been deleted?  */
  uint8_t accurate;    /* The inode is obsolete if accurate == 0.  */
  uint32_t dchksum;    /* Checksum for the data.  */
  uint16_t nchksum;    /* Checksum for the name.  */
  uint16_t chksum;     /* Checksum for the raw_inode.  */
};


struct jffs_file
{
  struct jffs_raw_inode inode;
  char *name;
  unsigned char *data;
};


char *root_directory_name = NULL;
int fs_pos = 0;
int verbose = 0;

#define ENDIAN_HOST   0
#define ENDIAN_BIG    1
#define ENDIAN_LITTLE 2
int endian = ENDIAN_HOST;

static uint32_t jffs_checksum(void *data, int size);
void jffs_print_trace(const char *path, int depth);
int make_root_dir(FILE *fs, int first_ino, const char *root_dir_path,
		  int depth);
void write_file(struct jffs_file *f, FILE *fs, struct stat st);
void read_data(struct jffs_file *f, const char *path, int offset);
int mkfs(FILE *fs, const char *path, int ino, int parent, int depth);


static uint32_t
jffs_checksum(void *data, int size)
{
  uint32_t sum = 0;
  uint8_t *ptr = (uint8_t *)data;

  while (size-- > 0)
  {
    sum += *ptr++;
  }

  return sum;
}


void
jffs_print_trace(const char *path, int depth)
{
  int path_len = strlen(path);
  int out_pos = depth * JFFS_TRACE_INDENT;
  int pos = path_len - 1;
  char *out = (char *)alloca(depth * JFFS_TRACE_INDENT + path_len + 1);

  if (verbose >= 2)
  {
    fprintf(stderr, "jffs_print_trace(): path: \"%s\"\n", path);
  }

  if (!out) {
    fprintf(stderr, "jffs_print_trace(): Allocation failed.\n");
    fprintf(stderr, " path: \"%s\"\n", path);
    fprintf(stderr, "depth: %d\n", depth);
    exit(1);
  }

  memset(out, ' ', depth * JFFS_TRACE_INDENT);

  if (path[pos] == '/')
  {
    pos--;
  }
  while (path[pos] && (path[pos] != '/'))
  {
    pos--;
  }
  for (pos++; path[pos] && (path[pos] != '/'); pos++)
  {
    out[out_pos++] = path[pos];
  }
  out[out_pos] = '\0';
  fprintf(stderr, "%s\n", out);
}


/* Print the contents of a raw inode.  */
void
jffs_print_raw_inode(struct jffs_raw_inode *raw_inode)
{
	fprintf(stderr, "jffs_raw_inode: inode number: %u\n", raw_inode->ino);
	fprintf(stderr, "{\n");
	fprintf(stderr, "        0x%08x, /* magic  */\n", raw_inode->magic);
	fprintf(stderr, "        0x%08x, /* ino  */\n", raw_inode->ino);
	fprintf(stderr, "        0x%08x, /* pino  */\n", raw_inode->pino);
	fprintf(stderr, "        0x%08x, /* version  */\n", raw_inode->version);
	fprintf(stderr, "        0x%08x, /* mode  */\n", raw_inode->mode);
	fprintf(stderr, "        0x%04x,     /* uid  */\n", raw_inode->uid);
	fprintf(stderr, "        0x%04x,     /* gid  */\n", raw_inode->gid);
	fprintf(stderr, "        0x%08x, /* atime  */\n", raw_inode->atime);
	fprintf(stderr, "        0x%08x, /* mtime  */\n", raw_inode->mtime);
	fprintf(stderr, "        0x%08x, /* ctime  */\n", raw_inode->ctime);
	fprintf(stderr, "        0x%08x, /* offset  */\n", raw_inode->offset);
	fprintf(stderr, "        0x%08x, /* dsize  */\n", raw_inode->dsize);
	fprintf(stderr, "        0x%08x, /* rsize  */\n", raw_inode->rsize);
	fprintf(stderr, "        0x%02x,       /* nsize  */\n", raw_inode->nsize);
	fprintf(stderr, "        0x%02x,       /* nlink  */\n", raw_inode->nlink);
	fprintf(stderr, "        0x%02x,       /* spare  */\n",
		 raw_inode->spare);
	fprintf(stderr, "        %u,          /* rename  */\n",
		 raw_inode->rename);
	fprintf(stderr, "        %u,          /* deleted  */\n",
		 raw_inode->deleted);
	fprintf(stderr, "        0x%02x,       /* accurate  */\n",
		 raw_inode->accurate);
	fprintf(stderr, "        0x%08x, /* dchksum  */\n", raw_inode->dchksum);
	fprintf(stderr, "        0x%04x,     /* nchksum  */\n", raw_inode->nchksum);
	fprintf(stderr, "        0x%04x,     /* chksum  */\n", raw_inode->chksum);
	fprintf(stderr, "}\n");
}

static void write_val32(uint32_t *adr, uint32_t val)
{
  switch(endian) {
  case ENDIAN_HOST:
    *adr = val;
    break;
  case ENDIAN_LITTLE:
    *adr = cpu_to_le32(val);
    break;
  case ENDIAN_BIG:
    *adr = cpu_to_be32(val);
    break;
  }
}

static void write_val16(uint16_t *adr, uint16_t val)
{
  switch(endian) {
  case ENDIAN_HOST:
    *adr = val;
    break;
  case ENDIAN_LITTLE:
    *adr = cpu_to_le16(val);
    break;
  case ENDIAN_BIG:
    *adr = cpu_to_be16(val);
    break;
  }
}

static uint32_t read_val32(uint32_t *adr)
{
  uint32_t val = 0;

  switch(endian) {
  case ENDIAN_HOST:
    val = *adr;
    break;
  case ENDIAN_LITTLE:
    val = le32_to_cpu(*adr);
    break;
  case ENDIAN_BIG:
    val = be32_to_cpu(*adr);
    break;
  }
  return val;
}


/* This function constructs a root inode with no name and
   no data.  The inode is then written to the filesystem
   image.  */
int
make_root_dir(FILE *fs, int first_ino, const char *root_dir_path, int depth)
{
  struct jffs_file f;
  struct stat st;

  if (stat(root_dir_path, &st) < 0)
  {
    perror("stat");
    exit(1);
  }

  write_val32(&f.inode.magic, JFFS_MAGIC);
  write_val32(&f.inode.ino, first_ino);
  write_val32(&f.inode.pino, 0);
  write_val32(&f.inode.version, 1);
  write_val32(&f.inode.mode, st.st_mode);
  write_val16(&f.inode.uid, 0); /* root */
  write_val16(&f.inode.gid, 0); /* root */
  write_val32(&f.inode.atime, st.st_atime);
  write_val32(&f.inode.mtime, st.st_mtime);
  write_val32(&f.inode.ctime, st.st_ctime);
  write_val32(&f.inode.offset, 0);
  write_val32(&f.inode.dsize, 0);
  write_val32(&f.inode.rsize,0);
  f.inode.nsize = 0;
  /*f.inode.nlink = st.st_nlink;*/
  f.inode.nlink = 1;
  f.inode.spare = 0;
  f.inode.rename = 0;
  f.inode.deleted = 0;
  f.inode.accurate = 0;
  write_val32(&f.inode.dchksum, 0);
  write_val16(&f.inode.nchksum, 0);
  write_val16(&f.inode.chksum, 0);
  f.name = 0;
  f.data = 0;
  write_val16(&f.inode.chksum, jffs_checksum(&f.inode, sizeof(struct jffs_raw_inode)));
  f.inode.accurate = 0xff;
  write_file(&f, fs, st);
  if (verbose >= 1)
  {
    jffs_print_trace(root_dir_path, depth);
  }
  if (verbose >= 2)
  {
    jffs_print_raw_inode(&f.inode);
  }
  return first_ino;
}


/* This function writes a chunks of data.  A data chunk consists of a
   raw inode, perhaps a name and perhaps some data.  */
void
write_file(struct jffs_file *f, FILE *fs, struct stat st)
{
  int npad = JFFS_GET_PAD_BYTES(f->inode.nsize);
  int dpad = JFFS_GET_PAD_BYTES(read_val32(&f->inode.dsize));
  int size = sizeof(struct jffs_raw_inode) + f->inode.nsize + npad
             + read_val32(&f->inode.dsize) + dpad;
  unsigned char ff_data[] = { 0xff, 0xff, 0xff, 0xff };

  if (verbose >= 2)
  {
    fprintf(stderr, "***write_file()\n");
  }

  /* Write the raw inode.  */
  fwrite((void *)&f->inode, sizeof(struct jffs_raw_inode), 1, fs);

  /* Write the name.  */
  if (f->inode.nsize)
  {
    fwrite(f->name, 1, f->inode.nsize, fs);
    if (npad)
    {
      fwrite(ff_data, 1, npad, fs);
    }
  }

  /* Write the data.  */
  if (read_val32(&f->inode.dsize))
  {
    if (S_ISBLK(st.st_mode) || S_ISCHR(st.st_mode))
    {
      uint16_t tmp;

      switch(endian) {
        case ENDIAN_HOST:
          tmp = st.st_rdev;
          break;
        case ENDIAN_LITTLE:
          tmp = cpu_to_le16(st.st_rdev);
          break;
        case ENDIAN_BIG:
          tmp = cpu_to_be16(st.st_rdev);
          break;
      }
      fwrite((char *)&tmp, sizeof(st.st_rdev) / 4, 1, fs);
    }
    else
    {
      fwrite(f->data, 1, read_val32(&f->inode.dsize), fs);
    }
    if (dpad)
    {
      fwrite(ff_data, 1, dpad, fs);
    }
  }

  fs_pos += size;
  /* If the space left on the block is smaller than the size of an
     inode, then skip it.  */
}


void
read_data(struct jffs_file *f, const char *path, int offset)
{
  FILE *file;
  char *tot_path;
  int pos = 0;
  int r;

  if (verbose >= 2)
  {
    fprintf(stderr, "***read_data(): f: 0x%08x, path: \"%s\", offset: %u\r\n",
            (unsigned int)f, path, offset);
    fprintf(stderr, "             file's size: %u\n", read_val32(&f->inode.dsize));
  }

  if (!(f->data = (unsigned char *)malloc(read_val32(&f->inode.dsize))))
  {
    fprintf(stderr, "read_data(): malloc() failed! (*data)\n");
    exit(1);
  }

  if (!(tot_path = (char *)alloca(strlen(path) + f->inode.nsize + 1)))
  {
    fprintf(stderr, "read_data(): alloca() failed! (tot_path)\n");
    exit(1);
  }
  strcpy(tot_path, path);
  strncat(tot_path, f->name, f->inode.nsize);

  if (!(file = fopen(tot_path, "r")))
  {
    fprintf(stderr, "read_data(): Couldn't open \"%s\".\n", tot_path);
    exit(1);
  }

  if (fseek(file, offset, SEEK_SET) < 0)
  {
    fprintf(stderr, "read_data(): fseek failure: path = %s, offset = %u.\n",
            path, offset);
    exit(1);
  }

  while (pos < read_val32(&f->inode.dsize))
  {
    if ((r = fread(&f->data[pos], 1, read_val32(&f->inode.dsize) - pos, file)) < 0)
    {
      fprintf(stderr, "read_data(): fread failure (%s).\n", path);
      exit(1);
    }
    pos += r;
  }

  fclose(file);
}


/* This is the routine that constructs the filesystem image.  */
int
mkfs(FILE *fs, const char *path, int ino, int parent, int depth)
{
  struct dirent *dir_entry;
  DIR *dir;
  struct stat st;
  struct jffs_file f;
  int name_len;
  int pos = 0;
  int new_ino = ino;
  char *filename;
  int path_len = strlen(path);

  if (verbose >= 2)
  {
    fprintf(stderr, "***mkfs(): path: \"%s\"\r\n", path);
  }

  if (!(dir = opendir(path)))
  {
    perror("opendir");
    fprintf(stderr, "mkfs(): opendir() failed! (%s)\n", path);
    exit(1);
  }

  while ((dir_entry = readdir(dir)))
  {
    if (verbose >= 2)
    {
     fprintf(stderr, "mkfs(): name: %s\n", dir_entry->d_name);
    }
    name_len = strlen(dir_entry->d_name);

    if (((name_len == 1)
         && (dir_entry->d_name[0] == '.'))
        || ((name_len == 2)
            && (dir_entry->d_name[0] == '.')
            && (dir_entry->d_name[1] == '.')))
    {
      continue;
    }

    if (!(filename = (char *)alloca(path_len + name_len + 1)))
    {
      fprintf(stderr, "mkfs(): Allocation failed!\n");
      exit(0);
    }
    strcpy(filename, path);
    strcat(filename, dir_entry->d_name);

    if (verbose >= 2)
    {
      fprintf(stderr, "mkfs(): filename: %s\n", filename);
    }

    if (lstat(filename, &st) < 0)
    {
      perror("lstat");
      exit(1);
    }

    if (verbose >= 2)
    {
      fprintf(stderr, "mkfs(): filename: \"%s\", ino: %d, parent: %d\n",
              filename, new_ino, parent);
    }

    write_val32(&f.inode.magic, JFFS_MAGIC);
    write_val32(&f.inode.ino, new_ino);
    write_val32(&f.inode.pino, parent);
    write_val32(&f.inode.version, 1);
    write_val32(&f.inode.mode, st.st_mode);
    write_val16(&f.inode.uid, st.st_uid);
    write_val16(&f.inode.gid, st.st_gid);
    write_val32(&f.inode.atime, st.st_atime);
    write_val32(&f.inode.mtime, st.st_mtime);
    write_val32(&f.inode.ctime, st.st_ctime);
    write_val32(&f.inode.dsize, 0);
    write_val32(&f.inode.rsize, 0);
    f.inode.nsize = name_len;
    /*f.inode.nlink = st.st_nlink;*/
    f.inode.nlink = 1;
    f.inode.spare = 0;
    f.inode.rename = 0;
    f.inode.deleted = 0;
    f.inode.accurate = 0;
    write_val32(&f.inode.dchksum, 0);
    write_val16(&f.inode.nchksum, 0);
    write_val16(&f.inode.chksum, 0);
    if (dir_entry->d_name)
    {
      f.name = strdup(dir_entry->d_name);
    }
    else
    {
      f.name = 0;
    }

  repeat:
    write_val32(&f.inode.offset, pos);
    f.data = 0;
    f.inode.accurate = 0;
    if (S_ISREG(st.st_mode) && st.st_size)
    {
      if (st.st_size - pos < MAX_CHUNK_SIZE)
      {
	write_val32(&f.inode.dsize, st.st_size - pos);
      }
      else
      {
	write_val32(&f.inode.dsize, MAX_CHUNK_SIZE);
      }

      read_data(&f, path, pos);
      pos += read_val32(&f.inode.dsize);
    }
    else if (S_ISLNK(st.st_mode))
    {
      int linklen;
      char *linkdata = malloc(1000);
      if (!linkdata)
      {
        fprintf(stderr, "mkfs(): malloc() failed! (linkdata)\n");
        exit(1);
      }
      if ((linklen = readlink(filename, linkdata, 1000)) < 0)
      {
        free(linkdata);
        fprintf(stderr, "mkfs(): readlink() failed! f.name = \"%s\"\n",
                f.name);
        exit(1);
      }

      write_val32(&f.inode.dsize, linklen);
      f.data = (unsigned char *)linkdata;
      f.data[linklen] = '\0';
    }
    else if (S_ISBLK(st.st_mode) || S_ISCHR(st.st_mode))
    {
      write_val32(&f.inode.dsize, sizeof(st.st_rdev) / 4);
    }

    write_val16(&f.inode.chksum, 0);
    if (!S_ISBLK(st.st_mode) && !S_ISCHR(st.st_mode))
    {
      write_val32(&f.inode.dchksum, jffs_checksum((void *)f.data, read_val32(&f.inode.dsize)));
    }
    else
    {
      write_val32(&f.inode.dchksum, jffs_checksum((void *)&st.st_rdev, sizeof(st.st_rdev) / 4));
    }

    write_val16(&f.inode.nchksum, jffs_checksum((void *)f.name, f.inode.nsize));
    write_val16(&f.inode.chksum, jffs_checksum((void *)&f.inode, sizeof(struct jffs_raw_inode)));
    f.inode.accurate = 0xff;

    write_file(&f, fs, st);
    if (S_ISREG(st.st_mode) && st.st_size)
    {
      if (pos < st.st_size)
      {
	write_val32(&f.inode.version, read_val32(&f.inode.version) + 1);
	goto repeat;
      }
    }

    new_ino++;
    pos = 0;
    if (verbose >= 1)
    {
      jffs_print_trace(f.name, depth);
    }
    if (verbose >= 2)
    {
      jffs_print_raw_inode(&f.inode);
    }

    if (S_ISDIR(st.st_mode))
    {
      char *new_path;

      if (!(new_path = (char *)alloca(strlen(path) + name_len + 1 + 1)))
      {
        fprintf(stderr, "mkfs(): alloca() failed! (new_path)\n");
        exit(1);
      }
      strcpy(new_path, path);
      strncat(new_path, f.name, f.inode.nsize);
      strcat(new_path, "/");

      if (verbose >= 2)
      {
        fprintf(stderr, "mkfs(): new_path: \"%s\"\n", new_path);
      }
      new_ino = mkfs(fs, new_path, new_ino, new_ino - 1, depth + 1);
    }
    if (f.name)
    {
      free(f.name);
    }
    if (f.data)
    {
      free(f.data);
    }
  }

  closedir(dir);
  return new_ino;
}


void
usage(void)
{
  fprintf(stderr, "Usage: mkfs.jffs -d root_directory [-a little|big] [-e erase_size] [-o output_file] [-v[0-9]]\n");
  fprintf(stderr, "       By default, the file system is built using the same endianness as the\n");
  fprintf(stderr, "       host.  If building for a different target, use the -a option.\n");
}


int
main(int argc, char **argv)
{
  FILE *fs;
  int root_ino;
  int len;
  int ch;
  extern int optind;
  extern char *optarg;

  fs = stdout; /* Send constructed file system to stdout by default */

  while ((ch = getopt(argc, argv, "a:d:e:v::o:h?")) != -1) {
    switch((char)ch) {
    case 'd':
      len = strlen(optarg);
      root_directory_name = (char *)malloc(len + 2);
      memcpy(root_directory_name, optarg, len);
      if (root_directory_name[len - 1] != '/')
	{
	  root_directory_name[len++] = '/';
	}
      root_directory_name[len] = '\0';
      break;
    case 'v':
      if (!optarg || strlen(optarg) == 0) {
	verbose = 1;
      }
      else if (strlen(optarg) > 1 || !isdigit(optarg[0])) {
	fprintf(stderr, "verbose level must be between 0 and 9!\n");
	usage();
	exit(1);
      }
      else {
	verbose = strtol(optarg, NULL, 0);
      }
      break;
    case 'o':
      fs = fopen(optarg, "w");
      if (!fs) {
	fprintf(stderr, "unable to open file %s for output.\n", optarg);
	exit(1);
      }
      break;
    case 'a':
      if (strcmp(optarg, "little") == 0) {
	endian = ENDIAN_LITTLE;
      }
      else if (strcmp(optarg, "big") == 0) {
	endian = ENDIAN_BIG;
      }
      else {
	usage();
	exit(1);
      }
      break;
    case 'e':
      MAX_CHUNK_SIZE = strtol(optarg, NULL, 0) / 2;
      break;
    case 'h':
    case '?':
    default:
      usage();
      exit(0);
    }
  }

  if ((argc -= optind)) {
    usage();
    exit(1);
  }

  if (root_directory_name == NULL) {
    fprintf(stderr, "Error:  must specify a root directory\n");
    usage();
    exit(1);
  }

  if (verbose >= 1)
  {
    fprintf(stderr, "Constructing JFFS filesystem...\n");
  }
  root_ino = make_root_dir(fs, JFFS_MIN_INO, root_directory_name, 0);
  mkfs(fs, root_directory_name, root_ino + 1, root_ino, 1);

  fclose(fs);
  free(root_directory_name);
  exit(0);
}
