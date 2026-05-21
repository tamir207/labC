#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>

int main(int argc, char **argv) {
    int pipefd[2];
    pid_t child1_pid, child2_pid;
    
    char *ps_args[] = {"ps", "-xl", NULL};
    char *grep_args[] = {"grep", "5", NULL};

    if (pipe(pipefd) == -1) {
        perror("pipe failed");
        exit(EXIT_FAILURE);
    }

    fprintf(stderr, "(parent_process>forking…)\n");
    
    child1_pid = fork();
    if (child1_pid == -1) {
        perror("fork 1 failed");
        exit(EXIT_FAILURE);
    }

    if (child1_pid == 0) {
        fprintf(stderr, "(child1>redirecting stdout to the write end of the pipe…)\n");
        close(STDOUT_FILENO);
        dup(pipefd[1]);
        close(pipefd[1]);
        close(pipefd[0]);

        fprintf(stderr, "(child1>going to execute cmd: ps -xl)\n");
        execvp(ps_args[0], ps_args);
        
        perror("execvp ps failed");
        _exit(EXIT_FAILURE);
    }

    fprintf(stderr, "(parent_process>created process with id: %d)\n", child1_pid);
    fprintf(stderr, "(parent_process>closing the write end of the pipe…)\n");

    close(pipefd[1]);

    fprintf(stderr, "(parent_process>forking…)\n");
    
    child2_pid = fork();
    if (child2_pid == -1) {
        perror("fork 2 failed");
        exit(EXIT_FAILURE);
    }

    if (child2_pid == 0) {
        fprintf(stderr, "(child2>redirecting stdin to the read end of the pipe…)\n");
        close(STDIN_FILENO);
        dup(pipefd[0]);
        close(pipefd[0]);

        fprintf(stderr, "(child2>going to execute cmd: grep 5)\n");
        execvp(grep_args[0], grep_args);
        
        perror("execvp grep failed");
        _exit(EXIT_FAILURE);
    }

    fprintf(stderr, "(parent_process>created process with id: %d)\n", child2_pid);

    fprintf(stderr, "(parent_process>closing the read end of the pipe…)\n");
    close(pipefd[0]);

    fprintf(stderr, "(parent_process>waiting for child processes to terminate…)\n");
    waitpid(child1_pid, NULL, 0);
    waitpid(child2_pid, NULL, 0);

    fprintf(stderr, "(parent_process>exiting…)\n");
    return 0;
}