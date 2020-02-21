// implementation.c
// Ty Bergstrom
// CSCE 321
// HW4
// Write a filesystem in userspace impementation

// compile:
// gcc -Wall myfs.c implementation.c `pkg-config fuse --cflags --libs` -o myfs

// mount without backup file:
// ./myfs --size=10000000 ./fuse-mnt/

// mount with backup file:
// ./myfs --backupfile=test.myfs --size=100000 ./fuse-mnt/

// unmount:
// fusermount -u ./fuse-mnt

// remount with backup file:
// ./myfs --backupfile=test.myfs --size=100000 ./fuse-mnt/

// run with valgrind:
// valgrind --leak-check=full ./myfs [--backupfile=test.myfs] ~/fuse-mnt/ -f

// run with GDB:
// gdb --args ./myfs --size=100000 ./fuse-mnt/ -f
//    OR:
//    gdb --args ./myfs ~/fuse-mnt/ -f




/*

  MyFS: a tiny file-system written for educational purposes

  MyFS is 

  Copyright 2018-19 by

  University of Alaska Anchorage, College of Engineering.

  Contributors: Christoph Lauter
                ... and
                ...

  and based on 

  FUSE: Filesystem in Userspace
  Copyright (C) 2001-2007  Miklos Szeredi <miklos@szeredi.hu>

  This program can be distributed under the terms of the GNU GPL.
  See the file COPYING.

  gcc -Wall myfs.c implementation.c `pkg-config fuse --cflags --libs` -o myfs


sources:
https://engineering.facile.it/blog/eng/write-filesystem-fuse/





*/

#include <stddef.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <libgen.h>


/* The filesystem you implement must support all the 13 operations
   stubbed out below. There need not be support for access rights,
   links, symbolic links. There needs to be support for access and
   modification times and information for statfs.

   The filesystem must run in memory, using the memory of size 
   fssize pointed to by fsptr. The memory comes from mmap and 
   is backed with a file if a backup-file is indicated. When
   the filesystem is unmounted, the memory is written back to 
   that backup-file. When the filesystem is mounted again from
   the backup-file, the same memory appears at the newly mapped
   in virtual address. The filesystem datastructures hence must not
   store any pointer directly to the memory pointed to by fsptr; it
   must rather store offsets from the beginning of the memory region.

   When a filesystem is mounted for the first time, the whole memory
   region of size fssize pointed to by fsptr reads as zero-bytes. When
   a backup-file is used and the filesystem is mounted again, certain
   parts of the memory, which have previously been written, may read
   as non-zero bytes. The size of the memory region is at least 2048
   bytes.

   CAUTION:

   * You MUST NOT use any global variables in your program for reasons
   due to the way FUSE is designed.

   You can find ways to store a structure containing all "global" data
   at the start of the memory region representing the filesystem.

   * You MUST NOT store (the value of) pointers into the memory region
   that represents the filesystem. Pointers are virtual memory
   addresses and these addresses are ephemeral. Everything will seem
   okay UNTIL you remount the filesystem again.

   You may store offsets/indices (of type size_t) into the
   filesystem. These offsets/indices are like pointers: instead of
   storing the pointer, you store how far it is away from the start of
   the memory region. You may want to define a type for your offsets
   and to write two functions that can convert from pointers to
   offsets and vice versa.

   * You may use any function out of libc for your filesystem,
   including (but not limited to) malloc, calloc, free, strdup,
   strlen, strncpy, strchr, strrchr, memset, memcpy. However, your
   filesystem MUST NOT depend on memory outside of the filesystem
   memory region. Only this part of the virtual memory address space
   gets saved into the backup-file. As a matter of course, your FUSE
   process, which implements the filesystem, MUST NOT leak memory: be
   careful in particular not to leak tiny amounts of memory that
   accumulate over time. In a working setup, a FUSE process is
   supposed to run for a long time!

   It is possible to check for memory leaks by running the FUSE
   process inside valgrind:

   valgrind --leak-check=full ./myfs --backupfile=test.myfs ~/fuse-mnt/ -f

   However, the analysis of the leak indications displayed by valgrind
   is difficult as libfuse contains some small memory leaks (which do
   not accumulate over time). We cannot (easily) fix these memory
   leaks inside libfuse.

   * Avoid putting debug messages into the code. You may use fprintf
   for debugging purposes but they should all go away in the final
   version of the code. Using gdb is more professional, though.

   * You MUST NOT fail with exit(1) in case of an error. All the
   functions you have to implement have ways to indicate failure
   cases. Use these, mapping your internal errors intelligently onto
   the POSIX error conditions.

   * And of course: your code MUST NOT SEGFAULT!

   It is reasonable to proceed in the following order:

   (1)   Design and implement a mechanism that initializes a filesystem
         whenever the memory space is fresh. That mechanism can be
         implemented in the form of a filesystem handle into which the
         filesystem raw memory pointer and sizes are translated.
         Check that the filesystem does not get reinitialized at mount
         time if you initialized it once and unmounted it but that all
         pieces of information (in the handle) get read back correctly
         from the backup-file.

   (2)   Design and implement functions to find and allocate free memory
         regions inside the filesystem memory space. There need to be 
         functions to free these regions again, too. Any "global" variable
         goes into the handle structure the mechanism designed at step (1) 
         provides.

   (3)   Carefully design a data structure able to represent all the
         pieces of information that are needed for files and
         (sub-)directories.  You need to store the location of the
         root directory in a "global" variable that, again, goes into the 
         handle designed at step (1).

   (4)   Write __myfs_getattr_implem and debug it thoroughly, as best as
         you can with a filesystem that is reduced to one
         function. Writing this function will make you write helper
         functions to traverse paths, following the appropriate
         subdirectories inside the file system. Strive for modularity for
         these filesystem traversal functions.

   (5)   Design and implement __myfs_readdir_implem. You cannot test it
         besides by listing your root directory with ls -la and looking
         at the date of last access/modification of the directory (.).
         Be sure to understand the signature of that function and use
         caution not to provoke segfaults nor to leak memory.

   (6)   Design and implement __myfs_mknod_implem. You can now touch files 
         with 

         touch foo

         and check that they start to exist (with the appropriate
         access/modification times) with ls -la.

   (7)   Design and implement __myfs_mkdir_implem. Test as above.

   (8)   Design and implement __myfs_truncate_implem. You can now 
         create files filled with zeros:

         truncate -s 1024 foo

   (9)   Design and implement __myfs_statfs_implem. Test by running
         df before and after the truncation of a file to various lengths. 
         The free "disk" space must change accordingly.

   (10)  Design, implement and test __myfs_utimens_implem. You can now 
         touch files at different dates (in the past, in the future).

   (11)  Design and implement __myfs_open_implem. The function can 
         only be tested once __myfs_read_implem and __myfs_write_implem are
         implemented.

   (12)  Design, implement and test __myfs_read_implem and
         __myfs_write_implem. You can now write to files and read the data 
         back:

         echo "Hello world" > foo
         echo "Hallo ihr da" >> foo
         cat foo

         Be sure to test the case when you unmount and remount the
         filesystem: the files must still be there, contain the same
         information and have the same access and/or modification
         times.

   (13)  Design, implement and test __myfs_unlink_implem. You can now
         remove files.

   (14)  Design, implement and test __myfs_unlink_implem. You can now
         remove directories.

   (15)  Design, implement and test __myfs_rename_implem. This function
         is extremely complicated to implement. Be sure to cover all 
         cases that are documented in man 2 rename. The case when the 
         new path exists already is really hard to implement. Be sure to 
         never leave the filessystem in a bad state! Test thoroughly 
         using mv on (filled and empty) directories and files onto 
         inexistant and already existing directories and files.

   (16)  Design, implement and test any function that your instructor
         might have left out from this list. There are 13 functions 
         __myfs_XXX_implem you have to write.

   (17)  Go over all functions again, testing them one-by-one, trying
         to exercise all special conditions (error conditions): set
         breakpoints in gdb and use a sequence of bash commands inside
         your mounted filesystem to trigger these special cases. Be
         sure to cover all funny cases that arise when the filesystem
         is full but files are supposed to get written to or truncated
         to longer length. There must not be any segfault; the user
         space program using your filesystem just has to report an
         error. Also be sure to unmount and remount your filesystem,
         in order to be sure that it contents do not change by
         unmounting and remounting. Try to mount two of your
         filesystems at different places and copy and move (rename!)
         (heavy) files (your favorite movie or song, an image of a cat
         etc.) from one mount-point to the other. None of the two FUSE
         processes must provoke errors. Find ways to test the case
         when files have holes as the process that wrote them seeked
         beyond the end of the file several times. Your filesystem must
         support these operations at least by making the holes explicit 
         zeros (use dd to test this aspect).

   (18)  Run some heavy testing: copy your favorite movie into your
         filesystem and try to watch it out of the filesystem.

*/

/* Helper types and functions */













// notice:
// N0T-U$ED denotes commented out functions that could be used later
//      depends on implementation choices
// P$EUD0 denotes useful pseudo code worth saving in case implementations change
// other comments are just normal comments
// there is no old buggy commented out code

#define MAGIC_NUM ((size_t) 17103563)
// the 7th prime number cat the 17th prime number cat the 103rd prime number

#define MAX_NAME ((int) 256+1) // +1 just for safety
// 256 = MAX_NAME-1, 255 = MAX_NAME-2, etc

#define DIRECTORY_TYPE ((int) 1)
#define FILE_TYPE ((int) 0)

// for parsing *path
// https://linux.die.net/man/3/basename
#include <libgen.h>

typedef size_t off_type;

typedef struct {
    off_type root_dir; // offset to root dir
    uint32_t magic;
    off_type begining_of_free_mem; // offset to the zone of contiguous memory
    int free_blocks; // how many non contiguous blocks are in the list
    size_t free_contig_mem_size;
    off_type offset_to_first_freed_block; // first free non contiguous block in the list
    size_t llist_mem_size_total; // total size of free non contiguous blocks
} handle_header;

typedef struct {
    size_t mem_size; // size of the block for data not including the block header
    size_t* file_size; // same
    size_t total_size; // size + sizeof(mem_block)
    //off_type next_off;
    struct mem_block* next; // next block in the list of freed non contiguous blocks
    struct mem_block* previous;
    char* name[MAX_NAME-1];
    //char type[1]; // 'd' or 'f'
    int type; // 0 for file, 1 for dir
    struct timespec st_size;
    struct timespec st_atim;
    struct timespec st_mtim;
    struct timespec *acc;
    struct timespec *mod;
    int num_children; // number of subdirectories of a directory
    int* num_subdir; // same
    struct mem_block* parent; // parent directory of a file or directory
    char parent_name[MAX_NAME-1];
    char path_name[MAX_NAME*10];
    char parent_path_name[MAX_NAME*9];
    int is_contiguous; // writing data to blocks is different if they are/not contiguous
    size_t total_non_contig_size; // a total_size for non contiguous blocks
    struct mem_block* next_non_contig_block; // to link non contiguous blocks
} mem_block;

off_type trans_to_off(void* fsptr, void* ptr){
    if(ptr==NULL)
        return (off_type) 0;
    if(ptr<=fsptr)
        return (off_type) 0;
    off_type offset = (off_type)ptr - (off_type)fsptr;
    return offset;
}

void* trans_to_ptr(void* fsptr, off_type offset){
    void* ptr = offset + fsptr;
    return ptr;
}

static void set_time(mem_block *block, int if_mod) {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    block->acc = &ts;
    if(if_mod==1)
        block->mod = &ts;
}

/*
// gdb was throwing errors for strcpy etc
char* str_cpy(char* dest, const char* src){
    if(dest==NULL)
        return NULL;
    memset(dest, '\0', MAX_NAME);
    char* ptr = dest;
    while(*src!='\0'){
        *dest = *src;
        dest++;
        src++;
    }
    *dest = '\0';
    return ptr;
}

int str_cmp(const char *a, const char *b){
    while(*a && *b && *a == *b){
        if(*a!=*b)
            break;
        a++;
        b++;
    }
    return *(const unsigned char*)a - *(const unsigned char*)b;
}
*/

static int check_name(char* name){
    int str_length = 0;
    char* i = name;
    while(*i != '\0'){
        str_length += 1;
        if(str_length > MAX_NAME)
            return 0;
        if(*i<32 || *i>122)
            return 0;
        i++;
    }
    if(str_length==0)
        return 0;
    return 1;
}

static int set_name(mem_block* block, char* name){
    if(check_name(name) == 0)
        return 0;
    if(block->type == DIRECTORY_TYPE)
        strcat(name, "/");
    strcpy(*block->name, name);
    return 1;
}

static mem_block* get_block(void*, size_t, size_t);
static mem_block* switch_to_llist_alloc(void*, size_t);
static mem_block* defrag_fs(void*, size_t, mem_block*);

static handle_header* init_fs(void* fsptr, size_t fssize){
    if(fssize<2048)
        return NULL;
    handle_header* handle = (handle_header*) fsptr;
    if(handle->magic == MAGIC_NUM)
        return handle;

    size_t size = fssize - sizeof(handle_header); // total size of mem
    if(size<sizeof(handle_header))
        return NULL;

    // initialize the memory zone
    //memset(fsptr + sizeof(handle_header), 0, size); // not sure which of these is better
    memset(fsptr, 0, fssize); // not sure which of these is right
    handle->free_contig_mem_size = size;
    // maintain offset to beginning of free memory zone
    handle->begining_of_free_mem = trans_to_off(fsptr, handle);
    handle->begining_of_free_mem += (off_type) sizeof(handle_header);

    // set up other handle metadata
    handle->magic = MAGIC_NUM;
    handle->free_blocks = (off_type) 0;

    // make the root directory
    mem_block* root_block = get_block(fsptr, 0, fssize);
    strcpy(*root_block->name, "/");
    strcpy(root_block->path_name, "/");
    root_block->type = DIRECTORY_TYPE;
    *(int*) (&root_block->num_subdir) = 0;
    root_block->num_children = 0;
    root_block->parent = NULL;

    return handle;
}

// the linked list of freed blocks was going to be used like the last homework
// but instead it will be used for non contiguous allocation
static mem_block* look_for_free_block(size_t size){
    //P$EUD0
    // iterate thru list
        // if size >= size
            // do not split
            // reset block->mem_size
            // calling function sets parent
            // reconnect linked list previous and next
            // free blocks -= 1
    return NULL;
}

static mem_block* get_block(void* fsptr, size_t size, size_t fssize){
    if(fsptr==NULL)
        return NULL;

    handle_header* handle = (handle_header*) fsptr;
    mem_block* block = look_for_free_block(size);
    if(block!=NULL){
        return block + sizeof(mem_block);

    // check that contiguous allocation is still possible
    // and get a block from the zone of contiguous memory
    }else if(size < handle->free_contig_mem_size){
        block = (mem_block*) trans_to_ptr(handle, handle->begining_of_free_mem);
        block->mem_size = size;
        block->total_size = size + sizeof(mem_block);
        block->file_size = (size_t*) size;

        set_time(block, 1);

        // split off this block from the zone of free contiguous memory
        handle->free_contig_mem_size -= block->total_size;
        handle->begining_of_free_mem += (off_type) block->total_size;

        block->is_contiguous = 1;
        return block + sizeof(mem_block);

    // else filesystem is possibly fragmented
    // switch to non contiguous allocation
    }else{
        block->is_contiguous = 0;
        block = switch_to_llist_alloc(fsptr, size);
        if(block!=NULL){
            set_time(block, 1);
            return block + sizeof(mem_block);

        // else still too fragmented, try defragmentaion
        }else{
            block = defrag_fs(fsptr, fssize, block);
            if(block!=NULL){
                set_time(block, 1);
                return block + sizeof(mem_block);

            // else just memory is full
            }else{
                return NULL;
            }
        }
    }
}

// add freed blocks to a linked list to recycle them for non contiguous allocation
static void free_block(void* fsptr, mem_block* block){
    handle_header* handle = (handle_header*) fsptr;
    // this becomes the first in a list of free blocks
    if(handle->free_blocks == 0){
        handle->free_blocks = trans_to_off(fsptr, block);
        handle->offset_to_first_freed_block = trans_to_off(fsptr, block);
    // or add it to the end of the list of free blocks
    }else{
        mem_block* current = trans_to_ptr(fsptr, handle->free_blocks);
        while(current->next != NULL){
            current = (mem_block*) current->next;
        }
        current->next = (void*) block;
    }
    handle->free_blocks++;
    handle->llist_mem_size_total += block->total_size;
    // don't overwrite header, blocks in the list still need to know their size
    memset(block+sizeof(mem_block), 0, block->mem_size);
    block->is_contiguous = 0;
    set_name(block, "\0");
}

/*NOT-U$ED
// changed my mind about how truncate works
static mem_block* reallocate(void* fsptr, size_t size, size_t fssize, mem_block* old_block){
    //P$EUD0
    // whether new size is bigger or smaller
        // get_block of new bigger or smaller size
        // free the old block
    mem_block* block = get_block(fsptr, size, fssize);
    free_block(fsptr, old_block);
    return block;
}
*/

static mem_block* follow_path(void* fsptr, const char* path){
    handle_header* handle = (handle_header*) fsptr;

    mem_block* root_dir = trans_to_ptr(fsptr, handle->root_dir);
    if(strcmp(path, *root_dir->name) == 0)
        return trans_to_ptr(fsptr, handle->root_dir);

    // start iterating from root directory
    off_type block_off = handle->root_dir;
    while(block_off < handle->begining_of_free_mem){
        // iterate to every block linearly
        mem_block* block = (mem_block*) trans_to_ptr(fsptr, block_off);
        // check if this block's saved path name is the given path
        if(strcmp(block->path_name, path)==0){
            return block;
        }else{
            // increment by the size of this block to get to the next header
            block_off += (off_type) block->total_size;
        }
    }
    return NULL; // did not find a basename that had the same path
}

// if fs is fragmented and there is not enough free
// contiguous memory to satisfy a request
// switch to a linked list allocation
static mem_block* switch_to_llist_alloc(void* fsptr, size_t size){
    handle_header* handle = (handle_header*) fsptr;
    // the handle knows how many free blocks are in the list, check it
    if(handle->free_blocks == 0)
        return NULL;
    // the handle knows the total size of all non contiguous blocks
    // check if there is enough space in all the blocks
    if(handle->llist_mem_size_total < size+sizeof(mem_block))
        return NULL;
    mem_block* block_itr = trans_to_ptr(fsptr, handle->offset_to_first_freed_block);
    // first just look for a single block that fits
    // replaces the look_for_free_block function (like last hw)
    int i=0;
    while(i < handle->free_blocks){
        // if found a block
        if(block_itr->total_size > size+sizeof(mem_block)){
            handle->free_blocks--;
            handle->llist_mem_size_total -= block_itr->total_size;
            block_itr->mem_size = size;
            // if this was the first block then update the offset to first block
            if(i==0){ 
                // but only if it wasn't the only block in the list
                if(handle->free_blocks > 1){
                    mem_block* new_head = (void*) block_itr->next;
                    handle->offset_to_first_freed_block = trans_to_off(fsptr, new_head);
                }
            // if it was in the middle of the list, reconnect the list
            }else{
                mem_block* prev = (void*) block_itr->previous;
                mem_block* nxt = (void*) block_itr->next;
                prev->next = (void*) nxt;
            }
            set_time(block_itr, 1);
            return block_itr + sizeof(mem_block);
        }else{
            if(block_itr->next == NULL)
                break;
            mem_block* next_block = (void*) block_itr->next;
            next_block->previous = (void*) block_itr;
            block_itr = (void*) block_itr->next;
            i++;
            continue;
        }
    }
    // or need to use non contiguous blocks to fit up to size

    //P$EUD0
    // iterate to each block
        // if first block allocated, save the total size in its header
            // block->total_non_contig_size = size
        // size -= block->mem_size
        // non_contig_size -= block->total_size
        // free_blocks -= 1
        // if size <= 0
            // break
        // reset head of list

    block_itr = trans_to_ptr(fsptr, handle->offset_to_first_freed_block);
    mem_block* first_block = block_itr;
    while(size>(size_t)0 || block_itr->next != NULL || handle->free_blocks > 0){
        size -= block_itr->mem_size;
        handle->llist_mem_size_total -= block_itr->total_size;
        handle->free_blocks--;
    }
    if(block_itr->next != NULL){
        mem_block* next_free_block = (void*) block_itr->next;
        handle->offset_to_first_freed_block = trans_to_off(fsptr, next_free_block->next);
        next_free_block->previous = NULL;
    }
    first_block->total_non_contig_size = size;
    set_time(first_block, 1);
    return first_block;
}

// if linked list allocation does not work for some reason,
// try to defrag the system, or just to see if this can be done
// by copying the fs to a new fs and restoring all used blocks contiguously
static mem_block* defrag_fs(void* fsptr, size_t fssize, mem_block* block){

    // i'm thinking of initializing a new fs
    // copy everything over
    // then re copy all blocks contiguously back to the first fs

    // also, the noncontiguous list of freed blocks may be fragmented.
    // need to split off fragmented space from the last freed block
    // that has been recycled, and add that to the new zone of free contiguous memory

    // old buggy code was deleted

    return NULL;
}

















/* End of helper functions */

/* Implements an emulation of the stat system call on the filesystem 
   of size fssize pointed to by fsptr.

   If path can be followed and describes a file or directory 
   that exists and is accessable, the access information is 
   put into stbuf

   On success, 0 is returned. On failure, -1 is returned and 
   the appropriate error code is put into *errnoptr.

   man 2 stat documents all possible error codes and gives more detail
   on what fields of stbuf need to be filled in. Essentially, only the
   following fields need to be supported:

   st_uid      the value passed in argument
   st_gid      the value passed in argument
   st_mode     (as fixed values S_IFDIR | 0755 for directories,
                                S_IFREG | 0755 for files)
   st_nlink    (as many as there are subdirectories (not files) for directories
                (including . and ..),
                1 for files)
   st_size     (supported only for files, where it is the real file size)
   st_atim
   st_mtim

*/

int __myfs_getattr_implem(void* fsptr, size_t fssize, int *errnoptr,
                          uid_t uid, gid_t gid,
                          const char *path, struct stat *stbuf) {
    if(path==NULL){
        *errnoptr = EBADF;
        return -1;
    }
    handle_header* handle = init_fs(fsptr, fssize);
    if(handle==NULL){
       *errnoptr = EFAULT;
        return -1;
    }
    mem_block* block = follow_path(fsptr, path);
    if(block==NULL){
        *errnoptr = ENOENT;
        return -1;
    }
    memset(stbuf, 0, sizeof(struct stat));
    stbuf->st_uid = uid;
    stbuf->st_gid = gid;

    // not sure which will work better
    //stbuf->st_atim = block->st_atim;
    //stbuf->st_mtim = block->st_mtim;

    stbuf->st_atim = *(struct timespec*) (&block->acc); 
    stbuf->st_mtim = *(struct timespec*) (&block->mod);

    if(block->type == DIRECTORY_TYPE){
        stbuf->st_mode = S_IFDIR | 0755;
        //stbuf->st_nlink = block->num_children;
        stbuf->st_nlink = *(int*) (&block->num_subdir);
    }else{
        stbuf->st_mode = S_IFREG | 0755;
        //stbuf->st_size = block->mem_size;
        stbuf->st_size = *(int*) (&block->file_size);
    }

    set_time(block, 0);
    return 0;
}

/* Implements an emulation of the readdir system call on the filesystem 
   of size fssize pointed to by fsptr. 

   If path can be followed and describes a directory that exists and
   is accessable, the names of the subdirectories and files
   contained in that directory are output into *namesptr. The . and ..
   directories must not be included in that listing.

   If it needs to output file and subdirectory names, the function
   starts by allocating (with calloc) an array of pointers to
   characters of the right size (n entries for n names). Sets
   *namesptr to that pointer. It then goes over all entries
   in that array and allocates, for each of them an array of
   characters of the right size (to hold the i-th name, together
   with the appropriate '\0' terminator). It puts the pointer
   into that i-th array entry and fills the allocated array
   of characters with the appropriate name. The calling function
   will call free on each of the entries of *namesptr and 
   on *namesptr.

   The function returns the number of names that have been 
   put into namesptr.

   If no name needs to be reported because the directory does
   not contain any file or subdirectory besides . and .., 0 is
   returned and no allocation takes place.

   On failure, -1 is returned and the *errnoptr is set to 
   the appropriate error code. 

   The error codes are documented in man 2 readdir.

   In the case memory allocation with malloc/calloc fails, failure is
   indicated by returning -1 and setting *errnoptr to EINVAL.

*/

int __myfs_readdir_implem(void *fsptr, size_t fssize, int *errnoptr,
                          const char *path, char ***namesptr) {
    if(path==NULL){
        *errnoptr = EBADF;
        return -1;
    }
    handle_header* handle = init_fs(fsptr, fssize);
    if(handle==NULL){
       *errnoptr = EFAULT;
        return -1;
    }
    mem_block* block = follow_path(fsptr, path);
    if(block==NULL){
        *errnoptr = ENOENT;
        return -1;
    }
    if(block->type != DIRECTORY_TYPE){
        set_time(block, 0);
        *errnoptr = ENOTDIR;
        return -1;
    }

    // iterate to every block linearly
    // check if its parent is the directory we are reading
    // if equal then add this file or dir to the buffer
    // this first for() loop just counts up how many names
    // don't see any other way to follow the instructions
    int names = 0;
    off_type block_off = handle->root_dir + (off_type) sizeof(mem_block);
    while(block_off < handle->begining_of_free_mem){
        mem_block* ch_block = (mem_block*) trans_to_ptr(handle, block_off);
        mem_block* parent = (mem_block*) ch_block->parent;
        set_time(ch_block, 0);
        set_time(parent, 0);
        if(parent->path_name == path){
            if(strcmp(".", *ch_block->name) == 0 || strcmp("..", *ch_block->name) == 0)
                continue;
            else{
                //namesptr[names] = (void*) ch_block->name;
                names++;
            }
        }
        block_off += (off_type) block->total_size;
    }

    set_time(block, 0);

    //If no name needs to be reported because the directory does
    //not contain any file or subdirectory besides . and .., 0 is
    //returned and no allocation takes place.
    if(names==0)
        return 0;

    //If it needs to output file and subdirectory names, the function
    //starts by allocating (with calloc) an array of pointers to
    //characters of the right size (n entries for n names).
    char** ptr_arr = calloc(names, sizeof(char*)*MAX_NAME);
    if(ptr_arr==NULL){
        *errnoptr = EINVAL;
        return -1;
    }

    //Sets *namesptr to that pointer.
    *namesptr = (void*) ptr_arr;
    //it then goes over all entries in that array
    names = 0;
    block_off = handle->root_dir + (off_type) sizeof(mem_block);
    while(block_off < handle->begining_of_free_mem){
        mem_block* child = (mem_block*) trans_to_ptr(handle, block_off);
        mem_block* parent = (mem_block*) child->parent;
        if(parent->path_name == path){
            if(strcmp(".", *child->name) == 0 || strcmp("..", *child->name) == 0){
                continue;
            }else{
                //and allocates, for each of them an array of
                //characters of the right size to hold the i-th name
                char** itr_arr = malloc(sizeof(char)*MAX_NAME);
                if(itr_arr==NULL){
                    *errnoptr = EINVAL;
                    return -1;
                }
                //It puts the pointer into that i-th array entry
                namesptr[names] = itr_arr;
                //and fills the allocated array of characters with the name
                //itr_arr = (void*) child->name;
                strcpy((void*) child->name, *namesptr[names]);

                names++;
            }
        }
        block_off += block->total_size;
    }
    return names;

    //P$EUD0
    // if not directory return -1
    // access directory
    // iterate through subdirectories and files
        // output to namesptr
        // int names++
    // namesptr = calloc(names, size of names?);
        // if namesptr == NULL set errnoptr return 0
    // iterate thru namesptr array
    // while(namesptr != NULL)
        // char* ptr = malloc(size of this name +1)
        // if ptr == NULL set errnoptr return -1
        // iterate through length of this name
        // ptr = name[i]
        // ptr ++
        // if last i
            // ptr = '/0'
            // break
        // namesptr++
    // return names

    // follow_path returns the mem_block* of this dir
    // check if it is a dir
    // iterate through every mem_block starting from root
        // if block->parent == dir
            // add it to namesptr
            // names++
}

/* Implements an emulation of the mknod system call for regular files
   on the filesystem of size fssize pointed to by fsptr.

   This function is called only for the creation of regular files.

   If a file gets created, it is of size zero and has default
   ownership and mode bits.

   The call creates the file indicated by path.

   On success, 0 is returned.

   On failure, -1 is returned and *errnoptr is set appropriately.

   The error codes are documented in man 2 mknod.

*/
int __myfs_mknod_implem(void *fsptr, size_t fssize, int *errnoptr,
                        const char *path) {
    if(path==NULL){
        *errnoptr = EBADF;
        return -1;
    }
    handle_header* handle = init_fs(fsptr, fssize);
    if(handle==NULL){
       *errnoptr = EFAULT;
        return -1;
    }
    // check if file already exists
    mem_block* block = follow_path(fsptr, path);
    if(block!=NULL){
        set_time(block, 0);
        *errnoptr = EEXIST;
        return -1;
    }
    // make the file, a new block of size zero
    mem_block* new_block = get_block(fsptr, (size_t) 0, fssize);
    // if not enough memory
    if(block==NULL){
        *errnoptr = EDQUOT;
        return -1;
    }

    // set path, name, parent name
    char* base_c = strdup(path);
    char* base_name = basename(base_c);
    char* dir_c = strdup(path);
    char* dir_name = dirname(dir_c);
    char* parent_c = strdup(path);
    char* parent_name = basename(parent_c);
    char* parentp_c = strdup(path);
    char* parent_path_name = basename(parentp_c);
    if(dir_name==NULL){
        *errnoptr = ENOENT;
        return -1;
    }
    int x = set_name(new_block, base_name);
    if(x!=1){
        *errnoptr = EAGAIN;
        return -1;
    }
    // assuming these have already been checked, or they wouldn't already be dir names
    //strcpy(new_block->name, base_name);
    strcpy(new_block->parent_name, parent_name);
    strcpy(new_block->path_name, path);
    strcpy(new_block->parent_path_name, parent_path_name);

    mem_block* parent_dir = follow_path(fsptr, parent_path_name);
    block->parent = (void*) parent_dir;

    new_block->type = FILE_TYPE;
    return 0;
}

/* Implements an emulation of the unlink system call for regular files
   on the filesystem of size fssize pointed to by fsptr.

   This function is called only for the deletion of regular files.

   On success, 0 is returned.

   On failure, -1 is returned and *errnoptr is set appropriately.

   The error codes are documented in man 2 unlink.

*/
int __myfs_unlink_implem(void *fsptr, size_t fssize, int *errnoptr,
                        const char *path) {
    if(path==NULL){
        *errnoptr = EBADF;
        return -1;
    }
    handle_header* handle = init_fs(fsptr, fssize);
    if(handle==NULL){
       *errnoptr = EFAULT;
        return -1;
    }
    mem_block* block = follow_path(fsptr, path);
    if(block==NULL){
        *errnoptr = ENOENT;
        return -1;
    }
    // unlinking a file just means freeing the block
    // there is nothing else that regular files have that needs to go away
    // update the directory's time modified though?
    mem_block* parent_dir = (void*) block->parent;
    set_time(parent_dir, 1);
    free_block(block, block);
    // now that block can be recycled
    return 0;
}

/* Implements an emulation of the rmdir system call on the filesystem 
   of size fssize pointed to by fsptr.

   The call deletes the directory indicated by path.

   On success, 0 is returned.

   On failure, -1 is returned and *errnoptr is set appropriately.

   The function call must fail when the directory indicated by path is
   not empty (if there are files or subdirectories other than . and ..).

   The error codes are documented in man 2 rmdir.

*/
int __myfs_rmdir_implem(void *fsptr, size_t fssize, int *errnoptr,
                        const char *path) {
    if(path==NULL){
        *errnoptr = EBADF;
        return -1;
    }
    handle_header* handle = init_fs(fsptr, fssize);
    if(handle==NULL){
       *errnoptr = EFAULT;
        return -1;
    }
    mem_block* block = follow_path(fsptr, path);
    if(block==NULL){
        *errnoptr = ENOENT;
        return -1;
    }
    // if not directory
    if(block->type != DIRECTORY_TYPE){
        *errnoptr = ENOTDIR;
        return -1;
    }
    // if root
    if(block->parent == NULL || strcmp(*block->name, "/")){
        *errnoptr = EACCES;
        return -1;
    }
    // if not empty
    if(block->num_children != 0){
        *errnoptr = EBUSY;
        return -1;
    }
    if((int*) (&block->num_subdir) != 0){
        *errnoptr = EBUSY;
        return -1;
    }else{
        // free the block just like unlink()
        // and update the parent dir
        mem_block* parent_dir = (mem_block*) block->parent;
        parent_dir->num_children -= 1;
        free_block(fsptr, block);
        set_time(parent_dir, 1);
        return 0;
    }
}

/* Implements an emulation of the mkdir system call on the filesystem 
   of size fssize pointed to by fsptr. 

   The call creates the directory indicated by path.

   On success, 0 is returned.

   On failure, -1 is returned and *errnoptr is set appropriately.

   The error codes are documented in man 2 mkdir.

*/
int __myfs_mkdir_implem(void *fsptr, size_t fssize, int *errnoptr,
                        const char *path) {
    if(path==NULL){
        *errnoptr = EBADF;
        return -1;
    }
    handle_header* handle = init_fs(fsptr, fssize);
    if(handle==NULL){
       *errnoptr = EFAULT;
        return -1;
    }
    // check if dir already exists
    mem_block* block = follow_path(fsptr, path);
    if(block!=NULL){
        set_time(block, 0);
        *errnoptr = EEXIST;
        return -1;
    }
    // make the dir, a new block of size zero
    mem_block* new_block = get_block(fsptr, (size_t) 0, fssize);
    // if not enough memory
    if(block==NULL){
        *errnoptr = EDQUOT;
        return -1;
    }

    // set path, name, parent name
    char* base_c = strdup(path);
    char* base_name = basename(base_c); // should look like "file"
    char* dir_c = strdup(path);
    char* dir_name = dirname(dir_c); // should be "/root/sub1/sub2/parent"
    char* parent_c = strdup(dir_name);
    char* parent_name = basename(parent_c); // should be "parent"
    if(dir_name==NULL){
        *errnoptr = ENOENT;
        return -1;
    }
    int x = set_name(new_block, base_name);
    if(x!=1){
        *errnoptr = EAGAIN;
        return -1;
    }
    //strcpy(new_block->name, base_name);
    strcpy(new_block->parent_name, parent_name);
    strcpy(new_block->path_name, path);
    strcpy(new_block->parent_path_name, dir_name);

    set_time(new_block, 1);

    new_block->type = DIRECTORY_TYPE;
    // do another follow path to get the parent block
    mem_block* parent_block = follow_path(fsptr, dir_name);
    parent_block->num_children += 1;
    *(int*) (&parent_block->num_subdir) += 1;
    new_block->parent = (void*) parent_block;

    set_time(parent_block, 0);
    return 0;
}

/* Implements an emulation of the rename system call on the filesystem 
   of size fssize pointed to by fsptr. 

   The call moves the file or directory indicated by from to to.

   On success, 0 is returned.

   On failure, -1 is returned and *errnoptr is set appropriately.

   Caution: the function does more than what is hinted to by its name.
   In cases the from and to paths differ, the file is moved out of 
   the from path and added to the to path.

   The error codes are documented in man 2 rename.

*/
int __myfs_rename_implem(void *fsptr, size_t fssize, int *errnoptr,
                         const char *from, const char *to) {

    //P$EUD0
    // first check if the paths are the same
    // follow path to get the file/dir indicated by *from
    // same for *to
    // check if it already exists in the *to path
    // get the basenames and parent names
    // replace block->path with the *to path
    // update the new and old parents
    // also check every dir in the paths
        // if they are different then update each... parent?
    // need to check if file or dir?

    if(strcmp(from, to) == 0)
        return 0;
    handle_header* handle = init_fs(fsptr, fssize);
    if(handle==NULL){
       *errnoptr = EFAULT;
        return -1;
    }
    mem_block* block = follow_path(fsptr, from);
    if(block==NULL){
        *errnoptr = ENOENT;
        return -1;
    }
    // check that already exists
    mem_block* to_block = follow_path(fsptr, to);
    if(to_block!=NULL){
        *errnoptr = EEXIST;
        return -1;
    }

    // update base name
    char* to_base_c = strdup(from);
    char* to_base_name = basename(to_base_c);
    memset(block->name, 0, MAX_NAME);
    strcpy(*block->name, to_base_name);

    // update block's path
    memset(block->path_name, 0, MAX_NAME*10);
    strcpy(block->path_name, to);

    // update every parent dir in the path
    // I'm not so sure about this
    //mem_block* block_itr;
    //while(strcmp(block_itr->name, "/") != 0){
        //break;
    //}

    // instead just update the parents at least
    // the new parent
    char* dir_c = strdup(to);
    char* dir_name = dirname(dir_c);
    char* to_parent_c = strdup(dir_name);
    char* to_parent_name = basename(to_parent_c);

    mem_block* to_parent_block = follow_path(fsptr, to_parent_name);
    to_parent_block->num_children += 1;
    *(int*) (&to_parent_block->num_subdir) += 1;
    block->parent = (void*) to_parent_block;

    // the old parent
    dir_c = strdup(from);
    dir_name = dirname(dir_c);
    char* from_parent_c = strdup(dir_name);
    char* from_parent_name = basename(from_parent_c);

    mem_block* from_parent_block = follow_path(fsptr, from_parent_name);
    from_parent_block->num_children -= 1;
    *(int*) (&from_parent_block->num_subdir) -= 1;

    set_time(to_parent_block, 1);
    set_time(from_parent_block, 1);
    set_time(block, 1);

    return 0;
}

/* Implements an emulation of the truncate system call on the filesystem 
   of size fssize pointed to by fsptr.

   The call changes the size of the file indicated by path to offset
   bytes.

   When the file becomes smaller due to the call, the extending bytes are
   removed. When it becomes larger, zeros are appended.

   On success, 0 is returned.

   On failure, -1 is returned and *errnoptr is set appropriately.

   The error codes are documented in man 2 truncate.

*/
int __myfs_truncate_implem(void *fsptr, size_t fssize, int *errnoptr,
                           const char *path, off_t offset) {
    if(path==NULL){
        *errnoptr = EBADF;
        return -1;
    }
    handle_header* handle = init_fs(fsptr, fssize);
    if(handle==NULL){
       *errnoptr = EFAULT;
        return -1;
    }
    mem_block* block = follow_path(fsptr, path);
    if(block==NULL){
        *errnoptr = ENOENT;
        return -1;
    }
    if(block->type == DIRECTORY_TYPE){
        set_time(block, 0);
        *errnoptr = EISDIR;
        return -1;
    }
    // if new size is less
    // create a free block out of the extra space
    if((size_t) offset < block->mem_size){
        mem_block* new_block = block + (off_type) offset;
        free(new_block);
        block->mem_size = (size_t) offset;
    }else{
        // make a new block of increased size, copy over its header info
        mem_block* new_block = get_block(fsptr, (size_t) offset, fssize);
        if(block==NULL){
            *errnoptr = EDQUOT;
            return -1;
        }
        strcpy(*new_block->name, *block->name);
        strcpy(new_block->parent_name, block->parent_name);
        strcpy(new_block->path_name, block->path_name);
        strcpy(new_block->parent_path_name, block->parent_path_name);
        // copy the contents of the old block to the new block

        // and append with zeros
        size_t zeros = (size_t) offset - block->mem_size;
        char* zeros_ar = malloc(zeros);
        memset(zeros_ar, 0, zeros);
        memcpy(new_block + block->mem_size, zeros_ar, strlen(zeros_ar));
        new_block->type = FILE_TYPE;
        free(zeros_ar);
        free_block(fsptr, block);
        new_block->type = FILE_TYPE;
    }
    return 0;

    //P$EUD0
    // if new size is smaller
        // split off the extra space past the offset
        // that extra space goes to the freed blocks list
    // else get a new bigger block
        // copy everything over to the new block
        // free the old block
}

/* Implements an emulation of the open system call on the filesystem 
   of size fssize pointed to by fsptr, without actually performing the opening
   of the file (no file descriptor is returned).

   The call just checks if the file (or directory) indicated by path
   can be accessed, i.e. if the path can be followed to an existing
   object for which the access rights are granted.

   On success, 0 is returned.

   On failure, -1 is returned and *errnoptr is set appropriately.

   The two only interesting error codes are 

   * EFAULT: the filesystem is in a bad state, we can't do anything

   * ENOENT: the file that we are supposed to open doesn't exist (or a
             subpath).

   It is possible to restrict ourselves to only these two error
   conditions. It is also possible to implement more detailed error
   condition answers.

   The error codes are documented in man 2 open.

*/
int __myfs_open_implem(void *fsptr, size_t fssize, int *errnoptr,
                       const char *path) {
    if(path==NULL){
        *errnoptr = EBADF;
        return -1;
    }
    handle_header* handle = init_fs(fsptr, fssize);
    if(handle==NULL){
       *errnoptr = EFAULT;
        return -1;
    }
    mem_block* block = follow_path(fsptr, path);
    if(block==NULL){
        *errnoptr = ENOENT;
        return -1;
    }
    set_time(block, 0);
    return 0;
}

/* Implements an emulation of the read system call on the filesystem 
   of size fssize pointed to by fsptr.

   The call copies up to size bytes from the file indicated by 
   path into the buffer, starting to read at offset. See the man page
   for read for the details when offset is beyond the end of the file etc.

   On success, the appropriate number of bytes read into the buffer is
   returned. The value zero is returned on an end-of-file condition.

   On failure, -1 is returned and *errnoptr is set appropriately.

   The error codes are documented in man 2 read.

*/
int __myfs_read_implem(void *fsptr, size_t fssize, int *errnoptr,
                       const char *path, char *buf, size_t size, off_t offset) {
    if(size==(size_t)0){
        return 0;
    }
    if(path==NULL){
        *errnoptr = EBADF;
        return -1;
    }
    handle_header* handle = init_fs(fsptr, fssize);
    if(handle==NULL){
       *errnoptr = EFAULT;
        return -1;
    }
    mem_block* block = follow_path(fsptr, path);
    if(block==NULL){
        *errnoptr = ENOENT;
        return -1;
    }
    if(block->type == DIRECTORY_TYPE){
        set_time(block, 0);
        *errnoptr = EISDIR;
        return -1;
    }

    char* buff = malloc(*(int*)(&block->mem_size));
    char *buff_cpy = buff;

    // according to the man page, if offset is beyond eof, I'm not sure
    // set the errnoptr and don't read any bytes
    off_type eof = trans_to_off(handle, block) + (off_type) block->total_size;
    if(offset > eof){
        *errnoptr = EFBIG;
        return 0;
    }
    // however it says if close to eof, then return less bytes than requested
    if(offset + (off_type) size >= eof){
        *errnoptr = EFBIG;
        size -= (offset+(off_type)size - eof);
    }

    buff_cpy += offset;
    int size_of_cpy = eof - offset;
    memcpy(buff, buff_cpy, size_of_cpy);
    free(buff);

    set_time(block, 0);
    return size_of_cpy;
}

/* Implements an emulation of the write system call on the filesystem 
   of size fssize pointed to by fsptr.

   The call copies up to size bytes to the file indicated by 
   path into the buffer, starting to write at offset. See the man page
   for write for the details when offset is beyond the end of the file etc.

   On success, the appropriate number of bytes written into the file is
   returned. The value zero is returned on an end-of-file condition.

   On failure, -1 is returned and *errnoptr is set appropriately.

   The error codes are documented in man 2 write.

*/
int __myfs_write_implem(void *fsptr, size_t fssize, int *errnoptr,
                        const char *path, const char *buf, size_t size, off_t offset) {

    //P$EUD0
    // iterate a pointer up to the offset
    // everything after this offset will be overwritten
    // malloc() the size of the region ... total_size minus offset
    // if writing at the end of the file, this would be zero
    // so possibly realloc() the additional size ... += size
    // memcpy( starting at the offset, move all of the buffer, of size)

    if(path==NULL){
        *errnoptr = EBADF;
        return -1;
    }
    handle_header* handle = init_fs(fsptr, fssize);
    if(handle==NULL){
       *errnoptr = EFAULT;
        return -1;
    }
    mem_block* block = follow_path(fsptr, path);
    if(block==NULL){
        *errnoptr = ENOENT;
    }
    if(block->type == DIRECTORY_TYPE){
        set_time(block, 0);
        *errnoptr = EISDIR;
        return -1;
    }
    // if offset is zero, replace everything
    if(offset == (off_t) 0){
        char* begin_data_field = (char*) block + sizeof(mem_block);
        char* replace = strndup(buf, size);
        memcpy(begin_data_field, replace, size);
        free(replace);
    }
    // if offset is beyond end of file
    if((off_type) offset > (off_type) (block + block->total_size)){
        *errnoptr = EFBIG;
        set_time(block, 0);
        return 0;
    }else{
        // append data to the end of a file, beginning at offset
        // for my implementations this means we will need to
        // make bigger mem_block, move everything over, and then append
        mem_block* new_block = get_block(fsptr, size+block->total_size, fssize);
        if(block==NULL){
            *errnoptr = EDQUOT;
            return -1;
        }
        //char* new_write = malloc(size);
        //memcpy(new_write+offset, buf, size);

        char* old_file = malloc(*(int*) block->file_size);
        char* new_file = strndup(old_file, offset);
        size_t new_size = size+offset;
        new_file = realloc(new_file, new_size);
        memcpy(new_file+offset, buf, size);
        // copy over to the new block
        char* begin_write = (char*) new_block + sizeof(mem_block);
        memcpy(begin_write, new_file, new_size);
        new_block->type = FILE_TYPE;
        free(new_file);
        free_block(fsptr, block);
    }
    set_time(block, 1);
    return size;
}

/* Implements an emulation of the utimensat system call on the filesystem 
   of size fssize pointed to by fsptr.

   The call changes the access and modification times of the file
   or directory indicated by path to the values in ts.

   On success, 0 is returned.

   On failure, -1 is returned and *errnoptr is set appropriately.

   The error codes are documented in man 2 utimensat.

*/
int __myfs_utimens_implem(void *fsptr, size_t fssize, int *errnoptr,
                          const char *path, const struct timespec ts[2]) {
    if(path==NULL){
        *errnoptr = EBADF;
        return -1;
    }
    handle_header* handle = init_fs(fsptr, fssize);
    if(handle==NULL){
       *errnoptr = EFAULT;
        return -1;
    }
    mem_block* block = follow_path(fsptr, path);
    if(block==NULL){
        *errnoptr = ENOENT;
        return -1;
    }

    memcpy(block->acc, &ts[0], sizeof(struct timespec));
    memcpy(block->mod, &ts[1], sizeof(struct timespec));

    return 0;
}

/* Implements an emulation of the statfs system call on the filesystem 
   of size fssize pointed to by fsptr.

   The call gets information of the filesystem usage and puts in 
   into stbuf.

   On success, 0 is returned.

   On failure, -1 is returned and *errnoptr is set appropriately.

   The error codes are documented in man 2 statfs.

   Essentially, only the following fields of struct statvfs need to be
   supported:

   f_bsize   fill with what you call a block (typically 1024 bytes)
   f_blocks  fill with the total number of blocks in the filesystem
   f_bfree   fill with the free number of blocks in the filesystem
   f_bavail  fill with same value as f_bfree
   f_namemax fill with your maximum file/directory name, if your
             filesystem has such a maximum

*/
int __myfs_statfs_implem(void *fsptr, size_t fssize, int *errnoptr,
                         struct statvfs* stbuf) {
    handle_header* handle = init_fs(fsptr, fssize);
    if(handle==NULL){
       *errnoptr = EFAULT;
        return -1;
    }
    stbuf->f_bsize = handle->free_contig_mem_size;
    stbuf->f_blocks = 1;
    stbuf->f_bfree = handle->free_blocks;
    stbuf->f_bavail = handle->free_blocks;
    stbuf->f_namemax = MAX_NAME;

    return -1;
}













/*
   (16)  Design, implement and test any function that your instructor
         might have left out from this list. There are 13 functions 
         __myfs_XXX_implem you have to write.

    http://linasm.sourceforge.net/docs/syscalls/filesystem.php
*/

// this isn't actually what was meant by (16) but it's still cool to think about

/*
       close() closes a file descriptor, so that it no longer refers to any
       file and may be reused.  Any record locks (see fcntl(2)) held on the
       file it was associated with, and owned by the process, are removed
       (regardless of the file descriptor that was used to obtain the lock).

       If fd is the last file descriptor referring to the underlying open
       file description (see open(2)), the resources associated with the
       open file description are freed; if the file descriptor was the last
       reference to a file which has been removed using unlink(2), the file
       is deleted.

       close() returns zero on success.  On error, -1 is returned, and errno
       is set appropriately.
*/

int __myfs_close_implem(void *fsptr, size_t fssize, int *errnoptr,
                       const char *path) {

    return -1;
}

/*
       Look up the full path of the directory entry specified by the value
       cookie.  The cookie is an opaque identifier uniquely identifying a
       particular directory entry.  The buffer given is filled in with the
       full path of the directory entry.

       For lookup_dcookie() to return successfully, the kernel must still
       hold a cookie reference to the directory entry.

       On success, lookup_dcookie() returns the length of the path string
       copied into the buffer.  On error, -1 is returned, and errno is set
       appropriately.
*/

int __myfs_lookup_dcookie_implem(void *fsptr, size_t fssize, int *errnoptr,
                       const char *path, int64_t cookie, char *buffer, size_t size) {

    return -1;
}

/*
       link() creates a new link (also known as a hard link) to an existing
       file.

       If newpath exists, it will not be overwritten.

       This new name may be used exactly as the old one for any operation;
       both names refer to the same file (and so have the same permissions
       and ownership) and it is impossible to tell which name was the
       "original".

       On success, zero is returned.  On error, -1 is returned, and errno is
       set appropriately.
*/

int __myfs_link_implem(void *fsptr, size_t fssize, int *errnoptr,
                       const char *old_path, const char *new_path) {

    return -1;
}






