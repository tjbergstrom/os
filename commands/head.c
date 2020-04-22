// head.c
// Ty Bergstrom
// to run this program use the following commands:
// gcc head.c
// gcc -o head head.c
// ./head
// ./head -n [num]
// ./head [filename] 
// ./head [filename] -n [num]

#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
 
int main(int argc, char * argv[]){
    int n = 0;
    int fd;
    int size = 128;
    char buf[size];

    // ************************
    // **** check for file ****
    // ************************
    int count = 1;
    while(count < argc){
        fd = open(argv[count], O_RDONLY);
        if(fd != -1){
            break;
        }else{
            count++;
            continue;
        }
    }

    // *******************************
    // **** check for n and set n ****
    // *******************************
    for(int i=0; i<argc; i++){
        if(strcmp(argv[i], "-n") == 0){
            if(i+1 >= argc){
                perror("Error, bad input");
                exit(1);
            }
            else{
                int length = 0;
                length = strlen(argv[i+1]);
                for(int j=0; j<length; j++)
                    n = n*10+(*argv[i+1]-'0');
                if(n > pow(2,64)){
                    perror("Error, n is too large");
                    exit(1);
                }
                if(n<=0)
                    exit(0);
            }
        }
    }

    // ************************************
    // ****** print n lines of input ******
    // ************************************
    if(argc == 1){
        n = 10;
    }
    if(argc==1 || argc==3){
        for(int i=0; i<n; i++){
            if(read(STDIN_FILENO, buf, size) == -1)
                perror("Error reading input");
            int j=0;
            while(j<size){
                if(write(STDOUT_FILENO, buf+j, sizeof(char)) == -1)
                    perror("Error writing input");
                j++;
                if(buf[j-1] == '\n' || buf[j-1] == '\0')
                    break;
                else
                    continue;
            }
        }
    }

    // *************************************
    // ****** print n lines from file ******
    // *************************************
    else if(fd==-1){
        perror("Error opening file");
        exit(1);
    }else{
        if(argc == 2){
            n = 10;
        }
        int file_size = lseek(fd, 0, SEEK_END);
        if(file_size < 0)
            perror("Error, file empty");
        lseek(fd, 0, SEEK_SET);
        char buff[file_size];
        if(read(fd, buff, file_size) == -1)
            perror("Error reading file");
        int j = 0;
        for(int i=0; i<n; i++){
            while(j<file_size){
                lseek(fd, 0, SEEK_SET);
                if(buff[j] == '\n' || buff[j] == '\0'){
                    write(STDOUT_FILENO, buff+j, sizeof(char));
                    j++;
                    break;
                }
                if(buff[j] != '\n' || buff[j] != '\0'){
                    write(STDOUT_FILENO, buff+j, sizeof(char));
                    j++;
                    continue;
                }
            }
        }
        if(close(fd) == -1)
            perror("Error closing flie");
    }

    // ******************
    // ****** done ******
    // ******************
    return 0;;
}


