// Copyright (C) 2013       Hykem <hykem@hotmail.com>
// Licensed under the terms of the GNU GPL, version 2
// http://www.gnu.org/licenses/gpl-2.0.txt

#include "ps4.h"
#include "defines.h"
#include "debug.h"
#include "unpfs.h"

#include "unpfs.h"

int pfs;
struct pfs_header_t *header;
struct di_d32 *inodes;

#define BUFFER_SIZE 65536

void memcpy_to_file(const char *fname, uint64_t ptr, uint64_t size)
{
  size_t bytes;
  size_t ix = 0;
  char *buffer = malloc(BUFFER_SIZE);
  if (buffer != NULL)
  {
    FILE *fp = fopen(fname, "wb");
    if (fp)
    {
      while (size > 0)
      {
         bytes = (size > BUFFER_SIZE) ? BUFFER_SIZE : size;
         lseek(pfs, ptr + ix * BUFFER_SIZE, SEEK_SET);
         read(pfs, buffer, bytes);
         fwrite(buffer, 1, bytes, fp);
         size -= bytes;
         ix++;
      }
      fclose(fp);
    }
    free(buffer);
  }
}

static void parse_directory(int ino, int lev, char *parent_name)
{
  for (uint32_t z = 0; z < inodes[ino].blocks; z++) 
  {
    uint32_t db = inodes[ino].db[0] + z;
    uint64_t pos = (uint64_t)header->blocksz * db;
    uint64_t size = inodes[ino].size;
    uint64_t top = pos + size;
    printfsocket("inode ino=0x%x db=0x%x pos=0x%"PRIx64" size=%"PRIu64"\n", ino, db, pos, size);
    while (pos < top)
    {
      struct dirent_t *ent = malloc (sizeof(struct dirent_t));
      lseek(pfs, pos, SEEK_SET);
      read(pfs, ent, sizeof(struct dirent_t));

      if (ent->type == 0)
      {
        free(ent);
        break;
      }

      char *name = malloc(ent->namelen + 1);
      memset(name, 0, ent->namelen + 1);
      if (lev > 0)
      {
        lseek(pfs, pos + sizeof(struct dirent_t), SEEK_SET);
        read(pfs, name, ent->namelen);
      }
      printfsocket(">dent ino=0x%x pos=0x%"PRIx64" name=%s\n", ent->ino, pos, name);

      char *fname = malloc(strlen (parent_name) + ent->namelen + 2);
      if (parent_name != NULL)
        sprintf(fname, "%s/%s", parent_name, name);
      else
        sprintf(fname, "%s", name);

      if ((ent->type == 2) && (lev > 0))
      {
        printfsocket(">file pos=0x%"PRIx64" size=%"PRId64" dest=%s\n",
               (uint64_t)header->blocksz * inodes[ent->ino].db[0],
               inodes[ent->ino].size, fname);
        memcpy_to_file(fname, (uint64_t)header->blocksz * inodes[ent->ino].db[0], inodes[ent->ino].size);
      }
      else
      if (ent->type == 3)
      {
        printfsocket(">scan dir %s\n", name);
        mkdir(fname, 0777);
        parse_directory(ent->ino, lev + 1, fname);
      }

      pos += ent->entsize;

      free(ent);
      free(name);
      free(fname);
    }
  }
}

int unpfs(char *pfsfn, char *tidpath)
{
  mkdir(tidpath, 0777);

  pfs = open(pfsfn, O_RDONLY, 0);
  if (pfs < 0) return -1;

  header = malloc(sizeof(struct pfs_header_t));
  lseek(pfs, 0, SEEK_SET);
  read(pfs, header, sizeof(struct pfs_header_t));

  inodes = malloc(sizeof(struct di_d32) * header->ndinode);

  uint32_t ix = 0;

  for (uint32_t i = 0; i < header->ndinodeblock; i++)
  {		
    for (uint32_t j = 0; (j < (header->blocksz / sizeof(struct di_d32))) && (ix < header->ndinode); j++)
    {
      lseek(pfs, (uint64_t)header->blocksz * (i + 1) + sizeof(struct di_d32) * j, SEEK_SET);
      read(pfs, &inodes[ix], sizeof(struct di_d32));
      printfsocket("inode ino=0x%x pos=0x%"PRIx64" blocks=%d mode=0x%x size=%"PRIu64" uid=0x%x gid=0x%x\n",
             ix, (uint64_t)header->blocksz * (i + 1) + sizeof(struct di_d32) * j,
             inodes[ix].blocks, inodes[ix].mode, inodes[ix].size, inodes[ix].uid, inodes[ix].gid);
      ix++;       
    }
  }

  parse_directory(header->superroot_ino, 0, tidpath);

  free(inodes);

  close(pfs);
	
  return 0;
}
