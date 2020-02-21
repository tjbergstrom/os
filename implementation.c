// implementation.c
// Ty Bergstrom
// CSCE 321
// November 3, 2019
// Solution to Homework Assignment #3
// implement malloc(), calloc(), realloc(), and free()


#include <stdlib.h>
#include <stdio.h>
#include <string.h>


/*  

    Copyright 2018-19 by

    University of Alaska Anchorage, College of Engineering.

    All rights reserved.

    Contributors:  ...
		   ...                 and
		   Christoph Lauter

    See file memory.c on how to compile this code.

    Implement the functions __malloc_impl, __calloc_impl,
    __realloc_impl and __free_impl below. The functions must behave
    like malloc, calloc, realloc and free but your implementation must
    of course not be based on malloc, calloc, realloc and free.

    Use the mmap and munmap system calls to create private anonymous
    memory mappings and hence to get basic access to memory, as the
    kernel provides it. Implement your memory management functions
    based on that "raw" access to user space memory.

    As the mmap and munmap system calls are slow, you have to find a
    way to reduce the number of system calls, by "grouping" them into
    larger blocks of memory accesses. As a matter of course, this needs
    to be done sensibly, i.e. without wasting too much memory.

    You must not use any functions provided by the system besides mmap
    and munmap. If you need memset and memcpy, use the naive
    implementations below. If they are too slow for your purpose,
    rewrite them in order to improve them!

    Catch all errors that may occur for mmap and munmap. In these cases
    make malloc/calloc/realloc/free just fail. Do not print out any 
    debug messages as this might get you into an infinite recursion!

    Your __calloc_impl will probably just call your __malloc_impl, check
    if that allocation worked and then set the fresh allocated memory
    to all zeros. Be aware that calloc comes with two size_t arguments
    and that malloc has only one. The classical multiplication of the two
    size_t arguments of calloc is wrong! Read this to convince yourself:

    https://cert.uni-stuttgart.de/ticker/advisories/calloc.en.html

    In order to allow you to properly refuse to perform the calloc instead
    of allocating too little memory, the __try_size_t_multiply function is
    provided below for your convenience.

*/

#include <stddef.h>
#include <sys/mman.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* Predefined helper functions */

static void *__memset(void *s, int c, size_t n){
    unsigned char *p;
    size_t i;
    if(n == ((size_t) 0)) 
        return s;
    for(i=(size_t) 0, p=(unsigned char *)s; 
    i<=(n-((size_t) 1)); i++, p++){
        *p = (unsigned char) c;
    }
    return s;
}

static void *__memcpy(void *dest, const void *src, size_t n) {
    unsigned char *pd;
    const unsigned char *ps;
    size_t i;
    if(n == ((size_t) 0))
        return dest;
    for(i=(size_t) 0, pd=(unsigned char *)dest, ps=(const unsigned char *)src;
    i<=(n-((size_t) 1)); i++, pd++, ps++){
        *pd = *ps;
  }
  return dest;
}

/* Tries to multiply the two size_t arguments a and b.

   If the product holds on a size_t variable, sets the 
   variable pointed to by c to that product and returns a 
   non-zero value.

   Otherwise, does not touch the variable pointed to by c and 
   returns zero.

   This implementation is kind of naive as it uses a division.
   If performance is an issue, try to speed it up by avoiding 
   the division while making sure that it still does the right 
   thing (which is hard to prove).

*/
static int __try_size_t_multiply(size_t *c, size_t a, size_t b){
    size_t t, r, q;
    /* If any of the arguments a and b is zero, everthing works just fine. */
    if((a == ((size_t) 0)) || (a == ((size_t) 0))){
        *c = a * b;
        return 1;
    }

    /* Here, neither a nor b is zero. 
    We perform the multiplication, which may overflow, i.e. present
    some modulo-behavior.
    */
    t = a * b;

    /* Perform Euclidian division on t by a:
    t = a * q + r
    As we are sure that a is non-zero, we are sure
    that we will not divide by zero.
    */
    q = t / a;
    r = t % a;

    /* If the rest r is non-zero, the multiplication overflowed. */
    if (r != ((size_t) 0)) return 0;

    /* Here the rest r is zero, so we are sure that t = a * q.
    If q is different from b, the multiplication overflowed.
    Otherwise we are sure that t = a * b.
    */
    if (q != b) return 0;
    *c = t;
    return 1;
}

/* End of predefined helper functions */

/* PSEUDO CODE

struct mem_block {
	length, size, next
}
mem_block *head
malloc(size){
    mem_block header = get_block()
        update metadata
    if no block found:
        mmap() a new block
        set metadata
    return ptr to start of mapping
}
mem_block *get_block(size){
    iterate thru list
        if current length > size:
            split() leave the extra space
            return the free block
    return null if no block found
}
mem_block *split(*current){
	make a new mem_block
	update data for this block with the left over space
	update the current block with the reduced space and return it
}
free(ptr){
    add_block()
    possibly munmap()
}
void add_block(*header){
    iterate to end of list and add
    coalesce()
}
void coalesce(){
    mem_block current = head
    while(current):
        update current length, size, next
        current++
}

*/

/* Your helper functions 

   You may also put some struct definitions, typedefs and global
   variables here. Typically, the instructor's solution starts with
   defining a certain struct, a typedef and a global variable holding
   the start of a linked list of currently free memory blocks. That
   list probably needs to be kept ordered by ascending addresses.

*/

typedef struct {
    size_t length;
    size_t mmap_size;
    void *next;
} mem_block;
static mem_block *head;

static mem_block *split(mem_block *current){
    mem_block *extra_space = current;
    extra_space->length = current->length - current->mmap_size;
    extra_space->mmap_size = extra_space->length - sizeof(mem_block);
    extra_space->next = current->next;
    current->length -= (extra_space->length);
    return current;
}

static mem_block *get_block(size_t sz){
    if(head==NULL)
        return NULL;
    mem_block *current = head;
    while(current->next != NULL){
        if(current->length > sz){ // first fit
            return split(current);
        }else{
            current = current->next;
        }
    }
    return NULL;
}

static void coalesce(){
    return;
    mem_block *current = head;
    while(current->next != NULL){
        mem_block *next = current->next;
        current->length += next->length;
        current->mmap_size += next->mmap_size;
        current->next = next->next;
        __memset(current+1, '\0', current->mmap_size);
        current = current->next;
    }
}

static void add_block(mem_block *header){
    mem_block *current = head;
    while(current->next != NULL){
        current = current->next;
    }
    current->next = header;
    header->next = NULL;
    coalesce();
}

/* End of your helper functions */

/* Start of the actual malloc/calloc/realloc/free functions */

void __free_impl(void *);

void *__malloc_impl(size_t size){
    //if(size == (size_t)0)
        //return NULL;
    //if(size > pow(2,64))
        //return NULL;
    mem_block *header1 = get_block(size); // look for a free block to use
    if(header1!=NULL){
        __memset((void*)(header1), '\0', size);
        header1->mmap_size = size;
        header1->next = NULL;
        return (void*)(header1+sizeof(mem_block));
    }else{ // or make a new mmap block
        size_t length = size + sizeof(mem_block);
        if(length<size) // overflow error
            return NULL;
        void *ptr = mmap(NULL,length,PROT_WRITE|PROT_READ,MAP_ANONYMOUS|MAP_PRIVATE,0,0);
        if(ptr==MAP_FAILED){
            return NULL;
        }else if(ptr==NULL){
            return NULL;
        }else{
            __memset(ptr, '\0', length);
            mem_block *header = ptr;
            header->length = length;
            header->mmap_size = size;
            header->next = NULL;
            return (void*)(header+sizeof(mem_block));
        }
    }
}

void *__calloc_impl(size_t nmemb, size_t size){
    if(size==0 || nmemb==0)
        return __malloc_impl(0);
    size_t sz=0;
    size_t *length = &sz;
    if(__try_size_t_multiply(length, nmemb, size)!=1)
        return NULL;
    void *ptr = __malloc_impl(*length);
    if(ptr==NULL){
        return NULL;
    }else{
        __memset(ptr, 0, *length);
        return ptr;
    }
}

void *__realloc_impl(void *ptr, size_t size){
    if(ptr==NULL)
        return __malloc_impl(size);
    if(size==((size_t)0)){
        __free_impl(ptr);
        return NULL;
    }
    void *new_ptr = __malloc_impl(size);
    if(new_ptr==NULL)
        return NULL;
    mem_block *header = (mem_block*)ptr - sizeof(mem_block);
    size_t length = header->mmap_size;
    if(size<length)
        length = size;
    __memcpy(new_ptr, ptr, length);
    __free_impl(ptr);
    return new_ptr;
}

void __free_impl(void *ptr){
    if(ptr==NULL)
        return;
    mem_block *header = (mem_block*)ptr - sizeof(mem_block);
    if(header==NULL){
        return;
    }else if(header->length > 1048576){
        munmap(ptr, header->length);
    }else if(head==NULL){
        head = header;
        header->next = NULL;
    }else
        add_block(header);
}

/* End of the actual malloc/calloc/realloc/free functions */


