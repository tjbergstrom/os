// findlocationfast.c
// Ty Bergstrom
// to compile and run this program enter the following commands:
// gcc findlocation.c
// gcc -o findlocationfast findlocationfast.c
// ./findlocationfast [filename] [6 digit prefix]

#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <ctype.h>

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

    // *****************************************************************************
    // *************** get file size for binary search & error check ***************
    // *****************************************************************************
    int file_size = lseek(file_open, 0, SEEK_END);
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

    // ***************************************************************
    // *************** read() & binary search the file ***************
    // ***************************************************************
    char buff[6];
    int middle_prefix = 0;
    int first_line = 0;
    int last_line = file_size / 32; // iterate by 32 bits to go line by line
    int middle_line = (first_line + last_line) / 2;
    while(1){
        lseek(file_open, middle_line*32, SEEK_SET); // set offset to middle line
        if(read(file_open, buff, 6) == -1) // read first 6 bytes of line
            perror("Error reading file");
        middle_prefix = atoi(buff); // convert the 6 bytes to integer
        if(middle_prefix < prefix)
            first_line = middle_line + 1;
        else if(middle_prefix == prefix) // prefix has been found
            break;
        else if(middle_prefix > prefix)
            last_line = middle_line - 1;
        middle_line = (first_line + last_line) / 2;
        if(first_line > last_line){ // prefix was not found
            if(close(file_open) == -1)
                perror("Error closing file");
            exit(1);
        }
    }

    // **********************************************************************
    // *************** get location & edit its format & output ***************
    // **********************************************************************
    char buf[24];
    char loc[24];
    lseek(file_open, middle_line*32+6, SEEK_SET); // set offset to middle line +6 bytes
    if(read(file_open, buf, 24) == -1) // read() all 24 bytes to unformatted location
        perror("Error reading file");
    for(int i=0; i<24; i++){
        if(buf[i] != ' ') // do not add spaces to formatted location
            loc[i] = buf[i];
        else if(buf[i] != 24){
            if(buf[i+1] != ' ') // add spaces that are between characters
                loc[i] = buf[i];
        }
    }
    if(write(STDOUT_FILENO, loc, sizeof(loc)-9) == -1) // -9 gets rid of junk at end of array
        perror("Error writing");
    const char msg[] = "\n";
    if(write(STDOUT_FILENO, msg, sizeof(msg)-1) == -1)
        perror("Error writing");

    // *********************************************
    // *************** done, close() ***************
    // *********************************************
    if(close(file_open) == -1)
        perror("Error closing file");
    exit(0);
}


