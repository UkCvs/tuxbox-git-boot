/*
 * Squashfs - a compressed read only filesystem for Linux
 *
 * Copyright (c) 2002 Phillip Lougher <phillip@lougher.demon.co.uk>
 *
 * U-boot adaptation:
 * Copyright (c) 2004 Carsten Juttner <carjay@gmx.net>
 *
 * Modified for Squashfs2:
 * Copyright (c) 2004 Marcel Pommer <marsellus@gmx.net>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 *
 *
 */

/*
 * 2004-10-17:
 *
 * this code does not yet support fragments. so be sure to have
 * the kernel in a squashfs built without -always-use-fragments.
 * your kernel will probably not be < 64k in size ;-)
 *
 * squashfs version 1.x is not supported anymore, compatibility
 * code may be added.
 *
 *
 * 2004-10-22:
 *
 * fragments are now supported, so -always-use-fragments works.
 *
 * compatibility with squashfs 1.x has been declared nonsense.
 *
 */

#include <common.h>

#if (CONFIG_FS & CFG_FS_SQUASHFS)

#include <linux/types.h>
#include <squashfs/squashfs_fs.h>
#include <zlib.h>
#include <malloc.h>
#include <cmd_fs.h>

#ifdef SQUASHFS_TRACE
#define TRACE(s, args...)				printf("SQUASHFS: "s, ## args)
#else
#define TRACE(s, args...)				{}
#endif

#define ERROR(s, args...)				printf("SQUASHFS error: "s, ## args)

#define SERROR(s, args...)				if(!silent) printf("SQUASHFS error: "s, ## args)
#define WARNING(s, args...)				printf("SQUASHFS: "s, ## args)


int read_fragment_table(struct part_info *info, squashfs_super_block *sBlk, squashfs_fragment_entry **fragment_table);
squashfs_fragment_entry *frag_table;


int squashfs_read_super (struct part_info *info, squashfs_super_block *super)
{
	if (info->size<sizeof(squashfs_super_block))
		return 0;

	/* superblock offset is actually index SQUASHFS_START which is defined as 0 */
	memcpy (super,(unsigned char*)info->offset,sizeof(squashfs_super_block));

	if (super->s_magic==SQUASHFS_MAGIC_SWAP)
	{
		ERROR ("non-native-endian squashfs is not supported\n");
		return 0;
	}
	if (super->s_magic!=SQUASHFS_MAGIC)
		return 0;
	if ((super->s_major!=SQUASHFS_MAJOR)||
		(super->s_minor!=SQUASHFS_MINOR))
	{
		ERROR ("unsupported squashfs version %d.%d\n",super->s_major,super->s_minor);
		return 0;
	}

	TRACE ("fragments: %d\n", super->fragments);
	TRACE ("fragment_table_start: 0x%x\n", super->fragment_table_start);
	TRACE ("inode_table_start: 0x%x\n", super->inode_table_start);
	TRACE ("directory_table_start: 0x%x\n", super->directory_table_start);

	if (!read_fragment_table(info, super, &frag_table))
	{
		return 0;
	}

	return 1;
}


static void read_bytes(struct part_info *info, unsigned int offset, int size, char *buffer)
{
	TRACE("read_bytes @ 0x%x, size %d, buffer 0x%x\n", offset,size,buffer);
	/* grab them directly off the Flash */
	char *src = (char *)info->offset+offset;
	memcpy (buffer,src,size);
}

/* read (un)compressed block from partition starting from start */
static int read_block(struct part_info *info, unsigned int start, unsigned int *next, unsigned char *block, squashfs_super_block *sBlk, unsigned int *bytecount)
{
	unsigned short int c_byte;
	int offset;
	unsigned char check_data;
	short int compressed;
	unsigned int length;

	if (!bytecount)
	{
		read_bytes(info, start, 2, (char *)&c_byte);
		compressed = SQUASHFS_COMPRESSED(c_byte);
		length = SQUASHFS_COMPRESSED_SIZE(c_byte);
		offset = 2;
		if(SQUASHFS_CHECK_DATA(sBlk->flags))
		{
			offset = 3;
			read_bytes(info, start+2, 1, (char *)&check_data);
			TRACE("block @ 0x%x, check data: 0x%x\n", start, check_data);
		}
	}
	else
	{
		compressed = SQUASHFS_COMPRESSED_BLOCK(*bytecount);
		length = SQUASHFS_COMPRESSED_SIZE_BLOCK(*bytecount);
		offset = 0;
	}

	if(compressed) 
	{
		unsigned char buffer[SQUASHFS_FILE_SIZE];
		int res;
		long bytes = SQUASHFS_FILE_SIZE;

		TRACE("compressed block @ 0x%x, compressed size %d\n", start, length);
		read_bytes(info, start + offset, length, buffer);

		squashfs_uncompress_init();
		res = squashfs_uncompress_block(block, bytes, buffer, length);
		TRACE("compressed block @ 0x%x, uncompressed size %d\n", start, res);
		if(!res)
		{
			ERROR("zlib::uncompress failed\n");
			squashfs_uncompress_exit();
			return 0;
		}
		squashfs_uncompress_exit();
		if(next)
		{
			*next = start + offset + length;
		}
		return res;
	} 
	else 
	{
		TRACE("uncompressed block @ 0x%x, size %d\n", start, length);
		read_bytes(info, start + offset, length, block);
		if(next)
		{
			*next = start + offset + length;
		}
		return length;
	}
}


static int read_fragment_block(struct part_info *info, unsigned int start, unsigned char *block, squashfs_super_block *sBlk, unsigned int *bytecount, unsigned int frag_offset, unsigned int frag_size)
{
	short int compressed;
	unsigned int length;

	compressed = SQUASHFS_COMPRESSED_BLOCK(*bytecount);
	length = SQUASHFS_COMPRESSED_SIZE_BLOCK(*bytecount);

	if(compressed) 
	{
		unsigned char buffer[SQUASHFS_FILE_SIZE];
		unsigned char uncompressed_buffer[SQUASHFS_FILE_SIZE];
		int res;
		long bytes = SQUASHFS_FILE_SIZE;

		TRACE("compressed block @ 0x%x, compressed size %d\n", start, length);
		read_bytes(info, start, length, buffer);

		squashfs_uncompress_init();
		res = squashfs_uncompress_block(uncompressed_buffer, bytes, buffer, length);
		TRACE("compressed block @ 0x%x, uncompressed size %d\n", start, res);
		if(!res)
		{
			ERROR("zlib::uncompress failed\n");
			squashfs_uncompress_exit();
			return 0;
		}
		squashfs_uncompress_exit();
		memcpy(block, uncompressed_buffer, frag_size);
		return frag_size;
	} 
	else 
	{
		TRACE("uncompressed block @ 0x%x, size %d\n", start, length);
		read_bytes(info, start + frag_offset, frag_size, block);
		return frag_size;
	}
}


int read_fragment_table(struct part_info *info, squashfs_super_block *sBlk, squashfs_fragment_entry **fragment_table)
{
	int i, indexes = SQUASHFS_FRAGMENT_INDEXES(sBlk->fragments);
	squashfs_fragment_index fragment_table_index[indexes];

	TRACE("reading %d fragment indexes from 0x%x\n", indexes, sBlk->fragment_table_start);
	if(sBlk->fragments == 0)
		return 1;

	if((*fragment_table = (squashfs_fragment_entry *) malloc(sBlk->fragments * sizeof(squashfs_fragment_entry))) == NULL) 
	{
		ERROR("Failed to allocate fragment table\n");
		return 0;
	}

	read_bytes(info, sBlk->fragment_table_start, SQUASHFS_FRAGMENT_INDEX_BYTES(sBlk->fragments), (char *) fragment_table_index);

	for(i = 0; i < indexes; i++) 
	{
		TRACE("reading fragment table block @ 0x%x\n", fragment_table_index[i]);
		int length = read_block(info, fragment_table_index[i], NULL, ((unsigned char *) *fragment_table) + (i * SQUASHFS_METADATA_SIZE), sBlk, NULL);
	}

	return 1;
}


/* reads directory header(s) and entries and looks for a given name. Returns the inode if found */
static int squashfs_readdir(struct part_info *info, squashfs_dir_inode_header *diri,
								char *filename, squashfs_inode *inode, squashfs_super_block *sBlk)
{
	squashfs_dir_header *dirh;
	char buffer[sizeof(squashfs_dir_entry) + SQUASHFS_NAME_LEN + 1];
	squashfs_dir_entry *dire = (squashfs_dir_entry *) buffer;
	int dir_count;
	unsigned int initial_offset = diri->offset;
	int bytes = 0;

	unsigned char *dirblock = malloc (SQUASHFS_METADATA_SIZE);
	unsigned int start = sBlk->directory_table_start + diri->start_block;
	unsigned int dirs=0;

	if (!dirblock)
	{
		ERROR ("squashfs_readdir: out of memory\n");
		return 0;
	}
	if (!read_block (info, start, NULL, dirblock, sBlk, NULL))
	{
		ERROR ("squashfs_readdir: read_block\n");
		free (dirblock);
		return 0;
	}
	while (bytes<diri->file_size)
	{
		dirh = (squashfs_dir_header*)(dirblock + initial_offset + bytes);
		dir_count = dirh->count + 1;
		dirs+=dir_count;
		bytes += sizeof(squashfs_dir_header);
		while(dir_count--) 
		{
			memcpy((void *)dire, dirblock + initial_offset + bytes, sizeof(dire));
			bytes += sizeof(*dire);
			memcpy((void *)dire->name, dirblock + initial_offset + bytes, dire->size + 1);
			dire->name[dire->size + 1] = '\0';
			if (!filename)
			{
				printf ("entry is %s\n", dire->name);
			}
			else if (filename && inode && !strncmp(dire->name,filename,dire->size+1))
			{
				TRACE ("entry found: %s\n", dire->name);
				*inode = SQUASHFS_MKINODE(dirh->start_block,dire->offset);
				free (dirblock);
				return 1;
			}
			bytes += dire->size + 1;
		}
	}
	if (!filename)
	{
		printf ("%d directory entries\n", dirs);
	}
	free (dirblock);
	return 0;
}

/*
 *  looks up a directory or file and displays information or loads the file
 */
#define SQUASHFS_FLAGS_LS	1
#define SQUASHFS_FLAGS_LOAD	2
static unsigned int squashfs_lookup (struct part_info *info, char *entryname, char flags,unsigned int loadoffset, unsigned int *size){
	squashfs_super_block sBlk;

	unsigned int root_inode_start;
	unsigned int root_inode_offset;

	unsigned char *blockbuffer;
	unsigned int blocksize;
	unsigned int cur_ptr;
	unsigned int cur_offset;

	char *partname;
	unsigned char parsed=0;

	squashfs_inode inode;

	if (!squashfs_read_super(info,&sBlk))
	{
		return 0;
	}

	/* inode-block relates to the compressed data and offset to the uncompressed result */
	root_inode_start = sBlk.inode_table_start + SQUASHFS_INODE_BLK(sBlk.root_inode);
	root_inode_offset = SQUASHFS_INODE_OFFSET(sBlk.root_inode);

	TRACE ("root_inode @ %x:%x\n",root_inode_start,root_inode_offset);

	blockbuffer = malloc (SQUASHFS_METADATA_SIZE);
	if (!blockbuffer)
	{
		ERROR ("out of memory allocating blockbuffer\n");
		return 0;
	}

	/* we get the root-inode block as a starting point */
	cur_ptr = root_inode_start;
	cur_offset = root_inode_offset;

	/* get first part of entryname */
	partname = strtok (entryname, "/");
	while (1)
	{
	/* now get the correct directory table entry */
  		squashfs_dir_inode_header diri;

		/* first lookup the dir-inode (starting with root) */
		blocksize = read_block(info, cur_ptr, &cur_ptr, blockbuffer, &sBlk, NULL);
		if (!blocksize)
		{
			ERROR ("reading inode block");
			break;
		}
		memcpy (&diri,blockbuffer+cur_offset,sizeof(squashfs_dir_inode_header));

		if (diri.inode_type != SQUASHFS_DIR_TYPE)	/* oops, we tried to dive too deep */
		{
			break;
		}

		/* second we lookup the actual entry in the directory table, if it matches we get back the inode */
		if (!squashfs_readdir (info, &diri, partname, &inode, &sBlk))
		{
			/* entry not found or root dir was requested */
			if (partname==NULL) parsed = 1;
			break;
		}
		cur_ptr = sBlk.inode_table_start + SQUASHFS_INODE_BLK(inode);
		cur_offset = SQUASHFS_INODE_OFFSET(inode);
		TRACE ("inode @ %x:%x\n",cur_ptr,cur_offset);
		if (partname)
		{
			partname = strtok (NULL,"/");
		}
		if (!partname)	/* no more to parse, either we have a dir or a file */
		{
			/* get the inode block from the inode_table */
			squashfs_base_inode_header base;
			blocksize = read_block(info, cur_ptr, &cur_ptr, blockbuffer, &sBlk, NULL);
			if (!blocksize)
			{
				ERROR ("reading final block");
				break;
			}
			/* check what kind of inode it is */
			memcpy (&base, blockbuffer + cur_offset,sizeof (base));
			if (flags&SQUASHFS_FLAGS_LS)
			{
				switch (base.inode_type)
				{
				case SQUASHFS_DIR_TYPE:
				{
					squashfs_dir_inode_header diri;
					memcpy (&diri, blockbuffer + cur_offset,sizeof (diri));
    					cur_ptr = sBlk.directory_table_start+diri.start_block;
					squashfs_readdir (info, &diri, NULL, NULL, &sBlk);
					break;
				}
				case SQUASHFS_FILE_TYPE:
					break;
				case SQUASHFS_SYMLINK_TYPE:
					/* resolve */
					break;
				default:
					break;
				}
				parsed = 1;
				break;
			} 
			else 
			{	/* SQUASHFS_FLAGS_LS */
				switch (base.inode_type)
				{
				case SQUASHFS_FILE_TYPE:
				{
					int i;
					unsigned int blocks;
					int frag_bytes;
					unsigned int *blocklist;
					unsigned int bytes = 0;
					int start;
					squashfs_reg_inode_header dirreg;

					memcpy (&dirreg,blockbuffer + cur_offset,sizeof(dirreg));
					
					blocks = dirreg.fragment == SQUASHFS_INVALID_BLK
									? (dirreg.file_size + sBlk.block_size - 1) >> sBlk.block_log
									: dirreg.file_size >> sBlk.block_log;
					frag_bytes = dirreg.fragment == SQUASHFS_INVALID_BLK ? 0 : dirreg.file_size % sBlk.block_size;
					start = dirreg.start_block;
					
					TRACE("regular file, size %d, blocks %d\n", dirreg.file_size, blocks);
					TRACE("frag_bytes %d, start_block 0x%x\n", frag_bytes, start);

					cur_offset += sizeof(dirreg);
					blocklist=malloc (blocks*sizeof(unsigned int));
					if (!blocklist)
					{
						ERROR("out of memory allocating blocklist\n");
						free (blockbuffer);
						return 0;
					}
					memcpy (blocklist,blockbuffer+cur_offset,blocks*sizeof(unsigned int));
					cur_ptr = dirreg.start_block;
					for (i=0;i<blocks;i++)
					{
						TRACE("reading block %d\n", i);
						bytes += read_block(info, cur_ptr, &cur_ptr, (unsigned char*)(loadoffset+bytes), &sBlk, blocklist+i);
					}
					if (frag_bytes)
					{
						squashfs_fragment_entry *frag_entry = frag_table + dirreg.fragment;
						TRACE("%d bytes in fragment %d, offset %d\n", frag_bytes, dirreg.fragment, dirreg.offset);
						TRACE("fragment %d, start_block=0x%x, size=%d\n",
							dirreg.fragment, frag_entry->start_block, SQUASHFS_COMPRESSED_SIZE_BLOCK(frag_entry->size));
						bytes += read_fragment_block(info, frag_entry->start_block, (unsigned char*)(loadoffset+bytes), &sBlk, &(frag_entry->size), dirreg.offset, frag_bytes);
					}
					*size=bytes;
					free (blocklist);
					break;
				}
				case SQUASHFS_SYMLINK_TYPE:
				{
					printf ("loading symlinks is not supported\n");
					free (blockbuffer);
					return 0;
					break;
				}
				}
			}
			parsed=1;
			break;
		}
	}

	free (blockbuffer);
	if (!parsed)
		return 0;
	else
		return 1;
}


int squashfs_ls (struct part_info *info, char *filename)
{
	char *name;
	
	TRACE("looking up %s\n", filename);
	
	if (!strncmp("/",filename,1)||*filename==0x00)
		name = NULL;
	else
		name = filename;
	if (filename[0]=='/')
		name = filename + 1;	/* ignore "/" at start - it's always root */

	if (!squashfs_lookup(info,name,SQUASHFS_FLAGS_LS,0,NULL))
	{
		ERROR ("name not found\n");
		return 0;
	}
	return 1;
}


int squashfs_info (struct part_info *info)
{
	squashfs_super_block super;

	if (!squashfs_read_super(info,&super))
	{
		ERROR ("reading superblock\n");
		return 0;
	}
	printf ("SquashFS version: %d.%d\n",super.s_major,super.s_minor);
	printf ("Files: %d\n",super.inodes);
	printf ("Bytes_used: %d\n",super.bytes_used);
	printf ("Block_size: %d\n",super.block_size);
	printf ("Inodes are %scompressed\n", SQUASHFS_UNCOMPRESSED_INODES(super.flags) ? "un" : "");
	printf ("Data is %scompressed\n", SQUASHFS_UNCOMPRESSED_DATA(super.flags) ? "un" : "");
	printf ("Fragments are %scompressed\n", SQUASHFS_UNCOMPRESSED_FRAGMENTS(super.flags) ? "un" : "");
	printf ("Check data is %spresent in the filesystem\n", SQUASHFS_CHECK_DATA(super.flags) ? "" : "not ");
	printf ("Fragments are %spresent in the filesystem\n", SQUASHFS_NO_FRAGMENTS(super.flags) ? "not " : "");
	printf ("Always_use_fragments option is %sspecified\n", SQUASHFS_ALWAYS_FRAGMENTS(super.flags) ? "" : "not ");
	printf ("Duplicates are %sremoved\n", SQUASHFS_DUPLICATES(super.flags) ? "" : "not ");
	printf ("Filesystem size %d bytes\n", super.bytes_used);
	printf ("Number of fragments %d\n", super.fragments);
	printf ("Number of inodes %d\n", super.inodes);
	printf ("Number of uids %d\n", super.no_uids);
	printf ("Number of gids %d\n", super.no_guids);

	return 1;
}


int squashfs_load (char *loadoffset, struct part_info *info, char *filename)
{
	unsigned int size = 0;

	if ((!strncmp("/",filename,1))||(filename[0]==0x00))
		return 0;
	if (!squashfs_lookup(info,filename,SQUASHFS_FLAGS_LOAD,(unsigned long)loadoffset,&size))
	{
		return 0;
	}
	return size;
}
#endif /* (CONFIG_FS & CFG_FS_SQUASHFS) */
