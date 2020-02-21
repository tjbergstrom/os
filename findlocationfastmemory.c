// findlocationfastmemory.c
// Ty Bergstrom
// Tristan Digert
// CSCE 321
// October 1, 2019
// Solution to Homework Assignment #2 Part 3
// to compile and run this program enter the following commands:
// gcc findlocation.c
// gcc -o findlocationfastmemory findlocationfastmemory.c
// ./findlocationfastmemory [filename] [6 digit prefix]

#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <ctype.h>
#include <sys/mman.h>
#include <assert.h>

int main(int argc , char *argv[]){

    // ***********************************************************
    // *************** open the file & error check ***************
    // ***********************************************************
    if(argc < 3){
        const char err_msg[] = "Error, file and/or prefix not entered\n";
        write(STDERR_FILENO, err_msg, sizeof(err_msg)-1);
        exit(1);
    }
    int count = 0;
    while(1){
        if(count == 6 && argv[2][count] == '\0')
            break; // prefix is correct
        if((argv[2][count] == '\0') || !isdigit(argv[2][count])){
            const char err_msg[] = "Error, prefix format incorrect\n";
            write(STDERR_FILENO, err_msg, sizeof(err_msg)-1);
            exit(1);
        }else
            count++;
            continue;
    }
    int prefix = atoi(argv[2]);
    int file_open = open(argv[1], O_RDONLY);
    if(file_open == -1){
        const char err_msg[] = "Error, file not opened\n";
        write(STDERR_FILENO, err_msg, sizeof(err_msg)-1);
        exit(1);
    }

    // **********************************************************************
    // *************** get file size for mmap() & error check ***************
    // **********************************************************************
    int file_size = lseek(file_open, 0, SEEK_END);
    size_t file_size_t = lseek(file_open, 0, SEEK_END);
    if(file_size % 32 != 0){
        const char err_msg[] = "Error, file is not correct type\n";
        write(STDERR_FILENO, err_msg, sizeof(err_msg)-1);
        if(close(file_open) == -1)
            perror("Error closing file");
        exit(1);
    }
    if(file_size == (off_t) - 1 || file_size == 0){
        const char err_msg[] = "Error, file is empty\n";
        write(STDERR_FILENO, err_msg, sizeof(err_msg)-1);
        if(close(file_open) == -1)
            perror("Error closing file");
        exit(1);
    }
    if(lseek(file_open, 0, SEEK_SET) < 0){
        const char err_msg[] = "Error, not a seekable file\n";
        write(STDERR_FILENO, err_msg, sizeof(err_msg)-1);
        if(close(file_open) == -1)
            perror("Error closing file");
        exit(1);
    }

    // ***********************************************
    // *************** mmap() the file ***************
    // ***********************************************
    char *mfile = mmap(0, file_size_t, PROT_READ, MAP_FILE | MAP_SHARED, file_open, 0);
    assert(mfile != MAP_FAILED);
    if(mfile == (caddr_t) -1){
        const char err_msg[] = "Error, mmap failed\n";
        write(STDERR_FILENO, err_msg, sizeof(err_msg)-1);
        if(close(file_open) == -1);
            perror("Error closing file");
        exit(1);
    }

    // ****************************************************************
    // *************** binary search the memory mapping ***************
    // ****************************************************************
    char buff[6];
    int middle_prefix = 0;
    int first_line = 0;
    int last_line = file_size / 32; 
    int middle_line = (first_line + last_line) / 2;
    mfile -= 32;
    while(1){
        mfile += middle_line * 32; // offest pointer to beginning of middle line
        for(int i=0; i<6; i++){ // read the middle prefix
            buff[i] = *mfile; // dereference these 6 addresses
            mfile++;
        }
        middle_prefix = atoi(buff); // convert to integer
        if(middle_prefix < prefix)
            first_line = middle_line + 1;
        else if(middle_prefix == prefix) // prefix has been found
            break;
        else if(middle_prefix > prefix)
            last_line = middle_line - 1;
        mfile -= middle_line * 32 + 6; // return pointer offset to beginning
        middle_line = (first_line + last_line) / 2;
        if(first_line > last_line){ // prefix was not found
            munmap(mfile, file_size_t);
            if(close(file_open) == -1)
                perror("Error closing file");
            exit(1);
        }
    }

    // *******************************************************************
    // *************** get location & edit format & output ***************
    // *******************************************************************
    char buf[18];
    for(int i=0; i<18; i++){ // pointer is already where we need it in memory
        if(*mfile != ' ') 
            buf[i] = *mfile; // dereference and add non space characters
        else if(i != 17)
            if(*++mfile != ' ') // dereference the next address, check if not space
                mfile--; // return to last address
                buf[i] = *mfile; // dereference and add this space
        mfile++;
    }
    if(write(STDOUT_FILENO, buf, sizeof(buf)-2) == -1)
        perror("Error writing");
    const char msg[] = "\n";
    if(write(STDOUT_FILENO, msg, sizeof(msg)-1) == -1)
        perror("Error writing");

    // ****************************************************
    // *************** munmap() and close() ***************
    // ****************************************************
    munmap(mfile, file_size_t);
    if(close(file_open) == -1)
        perror("Error closing file");
    exit(0);
}


