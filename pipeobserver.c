// Ty Bergstrom
// Tristan Digert
// pipeobserver.c
// CSCE 321
// Oct 7, 2019
// Solution to homework 2 part 4
// to compile and run this program enter the following commands:
// gcc pipeobserver.c
// gcc -o pipeobserver pipeobserver.c
// ./pipeobserver filename [ ls -lat ] [ wc -l ]

/***********************************************

PSEUDO CODE

int main()
	check if file opens

	parse args into first & second executables

	pipe()

	open(file)

	fork() child one
		close(file)
		dup() std out to W end of pipe
		close() R end of pipe
		execvp() first executable replaces child one
		exit(0) child one

	fork() child two
		dup() std in to R end of pipe
		close() W end of pipe

		pipe()

		fork() grandchild A
			dup() std out to W end of pipe
			close() R end of pipe
			read(stdin)
            write(stdout)
			write(file)
			close(file)
			exit(0) grandchild A

		fork() grandchild B
			close(file)
			dup() std in to R end of pipe
			close() W end of pipe
			execvp() second executable replaces g child B
			exit(0) grandchild B

		close(file)
		grandchild A and B have died
		exit(0) child two

	child one and two have died
    close(file)
	exit(0) parent

************************************************/

#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>

int main(int argc , char *argv[]){

    // **************
    // check the file
    // **************
    int file_open = open(argv[1], O_RDWR);
    if(file_open < 0){
        perror("Error, file not opened");
        exit(1);
    }
    if(close(file_open) == -1){
        perror("Error, file not closed");
        exit(1);
    }

    // ************************
    // **** parse the args ****
    // ************************
    char *exec1[8192];
    int cnt = 2;
    int arg1_cnt = 0;
    // first executable and args
    while(cnt<argc){
        if(strcmp(argv[cnt], "[") == 0){
            cnt++;
            continue;
        }else if(strcmp(argv[cnt], "]") == 0){
            cnt++;
            break;
        }else{
            int size_of_string = 0;
            size_of_string = strlen(argv[cnt]);
            char *input_string;
            input_string = malloc(size_of_string);
            strcat(input_string, argv[cnt]);
            if(arg1_cnt >= 8192){
                perror("Error, too many args to exec1");
                exit(1);
            }
            exec1[arg1_cnt] = input_string;
            arg1_cnt++;
            cnt++;
            continue;
        }
    }
    char *exec2[8192];
    int arg2_cnt = 0;
    // second exectuable and args
    while(cnt<argc){
        if(strcmp(argv[cnt], "[") == 0){
            cnt++;
            continue;
        }else if(strcmp(argv[cnt], "]") == 0){
            cnt++;
            break;
        }else{
            int size_of_string = 0;
            size_of_string = strlen(argv[cnt]);
            char *input_string;
            input_string = malloc(size_of_string);
            strcat(input_string, argv[cnt]);
            if(arg2_cnt >= 8192){
                perror("Error, too many args to exec2");
            }
            exec2[arg2_cnt] = input_string;
            arg2_cnt++;
            cnt++;
            continue;
        }
    }

    // ***************
    // create the pipe
    // ***************
    int pipes[2];
    if(pipe(pipes) < 0) {
        perror("Call to pipe() did not work");
        exit(1);
    }
    // parent opens the file, all children inherit this fd
    int fd = open(argv[1], O_RDWR);

    // **********************
    // fork() the first child
    // **********************
    pid_t child1_pid;
    child1_pid = fork();
    if(child1_pid < 0) {
        perror("Call to fork() did not work");
        exit(1);
    }else if(child1_pid == 0){
        // must close the fd that it inherited from the parent
        if(close(fd) == -1)
            perror("Error closing file");
        // close its std out
        if(close(STDOUT_FILENO) == -1)
            perror("Error closing file");
        // redirect its std out to write end of pipe
        if(dup(pipes[1]) == -1)
            perror("Error opening W end of pipe");
        // close its read end of the pipe
        if(close(pipes[0]) == -1)
            perror("Error closing R end of pipe");
        // close the parent's W fd
        if(close(pipes[1]) == -1)
            perror("Error closing pipe");

        // **************************************************
        // replace the child with the first executable to run
        // **************************************************
        if(execvp(exec1[0], exec1) == -1){
            perror("Error replacing child with exc");
            exit(1);
        }
        // close file passed to execvp
        if(close(*exec1[0]) == -1)
            perror("Error closing file");
        exit(0);
    }else if(child1_pid > 0){
        if(wait(NULL) == -1)
            perror("Error waiting");
    }
    // write a null char to the write end of the pipe
    // this will be the EOF condition for grandchild A
    char nullchr[] = "\0";
    if(write(pipes[1], nullchr, sizeof(nullchr)-1) == -1)
        perror("Error writing");

    // ***********************
    // fork() the second child
    // ***********************
    pid_t child2_pid;
    child2_pid = fork();
    if(child2_pid < 0) {
        perror("Child2 call to fork() did not work");
        exit(1);
    }else if(child2_pid == 0){
        // close its std in
        if(close(STDIN_FILENO) == -1)
            perror("Error closing file");
        // redirect its std in to read end of pipe
        if(dup(pipes[0]) == -1)
            perror("Error opening R end of pipe");
        // close its write end of the pipe
        if(close(pipes[1]) == -1)
            perror("Error closing W end of pipe");
        // close the parent's R fd
        if(close(pipes[0]) == -1)
            perror("Error closing pipe");

        // *************************
        // make another process pipe
        // *************************
        if(pipe(pipes) < 0) {
            perror("Call to pipe() did not work");
            exit(1);
        }

        // *******************
        // fork() grandchild A
        // *******************
        pid_t g_child_A;
        g_child_A = fork();
        if(g_child_A < 0) {
            perror("g_child_A call to fork() did not work");
            exit(1);
        }else if(g_child_A == 0){
            // close its std out
            if(close(STDOUT_FILENO) == -1)
                perror("Error closing file");
            // connect std out to write end of pipe
            if(dup(pipes[1]) == -1)
                perror("Error opening W end of pipe");
            // close its read end of the pipe
            if(close(pipes[0]) == -1)
                perror("Error closing R end of pipe");
            // reading and writing happens after g child B executes
            exit(0);
        }else if(g_child_A > 0){
            if(wait(NULL) == -1)
                perror("Error waiting");
        }

        // *******************
        // fork() grandchild B
        // *******************
        pid_t g_child_B;
        g_child_B = fork();
        if(g_child_B < 0) {
            perror("g_child_B call to fork() did not work");
            exit(1);
        }else if(g_child_B == 0){
            // must close the fd that it inherited from the parent
            if(close(fd) == -1)
                perror("Error closing file GB");
            // close its std in
            if(close(STDIN_FILENO) == -1)
                perror("Error closing file");
            // redirect its std in to read end of pipe
            if(dup(pipes[0]) == -1)
                perror("Error opening R end of pipe");
            // close its write end of the pipe
            if(close(pipes[1]) == -1)
                perror("Error closing W end of pipe");
            // close the parent's R fd
            if(close(pipes[0]) == -1)
                perror("Error closing pipe");

            // ***************************************************
            // replace g child B with the second executable to run
            // ***************************************************
            if(execvp(exec2[0], exec2) == -1){
                perror("Error replacing g child B with exc");
                exit(1);
            }
            // close the file passed to execvp
            if(close(*exec2[0]) == -1)
                perror("Error closing file");
            exit(0);
        }else if(g_child_B > 0){
            if(wait(NULL) == -1)
                perror("Error waiting");
        }

            // *******************************
            // grandchild A read() and write() 
            // *******************************
            char newLine[] = "\n\n";
            if(write(STDOUT_FILENO, newLine, sizeof(newLine)-1) == -1)
                perror("Error writing");
            char ch[33];
            for(int i=0; i<32; i++){
                if(read(STDIN_FILENO, ch, 32) == -1){
                    perror("Error reading file");
                    close(fd);
                    exit(0);
                }
                for(int c=0; c<33; c++){\
                    if(ch[c] == '\0')
                        exit(0);
                }
                if(write(STDOUT_FILENO, ch, sizeof(ch)-1) == -1)
                    perror("Error writing to std out");
                if(write(fd, ch, sizeof(ch)-1) == -1)
                    perror("Error writing to file");
            }
            if(write(STDOUT_FILENO, newLine, sizeof(newLine)-1) == -1)
                perror("Error writing");

        if(close(fd) == -1)
            perror("Error closing file");
        exit(0);
    }else if(child2_pid > 0){
        if(wait(NULL) == -1)
            perror("Error waiting");;
    }

    // ************************
    // parent process finishing
    // ************************
    if(close(fd) == -1)
        perror("Error closing");
    char end_msg[] = "\n\nAll processes finished\n";
    if(write(STDOUT_FILENO, end_msg, sizeof(end_msg)-1) == -1)
        perror("Error writing");
    exit(0);
}


