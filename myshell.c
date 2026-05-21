#define _GNU_SOURCE
#include "LineParser.h"
#include <fcntl.h>
#include <linux/limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#define TERMINATED -1
#define RUNNING 1
#define SUSPENDED 0

typedef struct process {
    cmdLine* cmd;
    pid_t pid;
    int status;
    struct process* next;
} process;

process* process_list = NULL;
int debug_mode = 0;

void addProcess(process** process_list, cmdLine* cmd, pid_t pid) {
    process* new_proc = (process*)malloc(sizeof(process));
    new_proc->cmd = cmd;
    new_proc->pid = pid;
    new_proc->status = RUNNING;
    new_proc->next = *process_list;
    *process_list = new_proc;
}

void updateProcessList(process** process_list) {
    process* curr = *process_list;
    while (curr != NULL) {
        int status;
        pid_t res = waitpid(curr->pid, &status, WNOHANG | WUNTRACED | WCONTINUED);
        if (res == -1) {
            curr->status = TERMINATED;
        } else if (res > 0) {
            if (WIFEXITED(status) || WIFSIGNALED(status)) {
                curr->status = TERMINATED;
            } else if (WIFSTOPPED(status)) {
                curr->status = SUSPENDED;
            } else if (WIFCONTINUED(status)) {
                curr->status = RUNNING;
            }
        }
        curr = curr->next;
    }
}

void printProcessList(process** process_list) {
    updateProcessList(process_list);
    printf("%-12s %-12s %s\n", "PID", "STATUS", "Command");
    process* curr = *process_list;
    process* prev = NULL;

    while (curr != NULL) {
        char* status_str = "Running";
        if (curr->status == TERMINATED) {
            status_str = "Terminated";
        } else if (curr->status == SUSPENDED) {
            status_str = "Suspended";
        }

        printf("%-12d %-12s ", curr->pid, status_str);
        for (int i = 0; i < curr->cmd->argCount; i++) {
            printf("%s ", curr->cmd->arguments[i]);
        }
        printf("\n");

        if (curr->status == TERMINATED) {
            process* to_delete = curr;
            if (prev == NULL) {
                *process_list = curr->next;
                curr = curr->next;
            } else {
                prev->next = curr->next;
                curr = curr->next;
            }
            freeCmdLines(to_delete->cmd);
            free(to_delete);
        } else {
            prev = curr;
            curr = curr->next;
        }
    }
}

void freeProcessList(process* process_list) {
    process* curr = process_list;
    while (curr != NULL) {
        process* to_delete = curr;
        curr = curr->next;
        freeCmdLines(to_delete->cmd);
        free(to_delete);
    }
}

void execute(cmdLine* pCmdLine) {
    char* cmd = pCmdLine->arguments[0];
    if (strcmp(cmd, "cd") == 0) {
        if (pCmdLine->argCount < 2) {
            fprintf(stderr, "cd: missing argument\n");
        } else if (chdir(pCmdLine->arguments[1]) == -1) {
            perror("cd failed");
        }
        return;
    } else if (strcmp(cmd, "procs") == 0) {
        printProcessList(&process_list);
        return;
    } else if (strcmp(cmd, "stop") == 0) {
        if (pCmdLine->argCount < 2) {
            fprintf(stderr, "stop: missing process id\n");
            return;
        }
        int targetPid = atoi(pCmdLine->arguments[1]);
        if (kill(targetPid, SIGSTOP) == -1) {
            perror("stop failed");
        }
        return;
    } else if (strcmp(cmd, "wakeup") == 0) {
        if (pCmdLine->argCount < 2) {
            fprintf(stderr, "wakeup: missing process id\n");
            return;
        }
        int targetPid = atoi(pCmdLine->arguments[1]);
        if (kill(targetPid, SIGCONT) == -1) {
            perror("wakeup failed");
        }
        return;
    } else if (strcmp(cmd, "ice") == 0) {
        if (pCmdLine->argCount < 2) {
            fprintf(stderr, "ice: missing process id\n");
            return;
        }
        int targetPid = atoi(pCmdLine->arguments[1]);
        if (kill(targetPid, SIGINT) == -1) {
            perror("ice failed");
        }
        return;
    } else if (strcmp(cmd, "nuke") == 0) {
        if (pCmdLine->argCount < 2) {
            fprintf(stderr, "nuke: missing process id\n");
            return;
        }
        int targetPid = atoi(pCmdLine->arguments[1]);
        if (kill(-targetPid, SIGKILL) == -1) {
            perror("nuke failed");
        }
        return;
    }

    if (pCmdLine->next != NULL) {
        int pipefd[2];
        if (pipe(pipefd) == -1) {
            perror("pipe failed");
            return;
        }

        pid_t pid1 = fork();
        if (pid1 == -1) {
            perror("fork failed");
            return;
        }

        if (pid1 == 0) {
            if (pCmdLine->inputRedirect != NULL) {
                int fd_in = open(pCmdLine->inputRedirect, O_RDONLY);
                if (fd_in == -1) {
                    perror("input redirection failed");
                    _exit(1);
                }
                dup2(fd_in, STDIN_FILENO);
                close(fd_in);
            }

            dup2(pipefd[1], STDOUT_FILENO);
            close(pipefd[1]);
            close(pipefd[0]);

            if (execvp(pCmdLine->arguments[0], pCmdLine->arguments) == -1) {
                perror("Execution failed");
                _exit(1);
            }
        }

        pid_t pid2 = fork();
        if (pid2 == -1) {
            perror("fork failed");
            return;
        }

        if (pid2 == 0) {
            if (pCmdLine->next->outputRedirect != NULL) {
                int fd_out = open(pCmdLine->next->outputRedirect, O_WRONLY | O_CREAT | O_TRUNC, 0644);
                if (fd_out == -1) {
                    perror("output redirection failed");
                    _exit(1);
                }
                dup2(fd_out, STDOUT_FILENO);
                close(fd_out);
            }

            dup2(pipefd[0], STDIN_FILENO);
            close(pipefd[0]);
            close(pipefd[1]);

            if (execvp(pCmdLine->next->arguments[0], pCmdLine->next->arguments) == -1) {
                perror("Execution failed");
                _exit(1);
            }
        }

        close(pipefd[0]);
        close(pipefd[1]);

        cmdLine* nextCmd = pCmdLine->next;
        pCmdLine->next = NULL;
        addProcess(&process_list, pCmdLine, pid1);
        addProcess(&process_list, nextCmd, pid2);

        if (pCmdLine->blocking) {
            waitpid(pid1, NULL, 0);
            waitpid(pid2, NULL, 0);
        }

    } else {
        pid_t pid = fork();
        if (pid == -1) {
            perror("fork failed");
            return;
        }

        if (pid == 0) {
            if (pCmdLine->inputRedirect != NULL) {
                int fd_in = open(pCmdLine->inputRedirect, O_RDONLY);
                if (fd_in == -1) {
                    perror("input redirection failed");
                    _exit(1);
                }
                dup2(fd_in, STDIN_FILENO);
                close(fd_in);
            }

            if (pCmdLine->outputRedirect != NULL) {
                int fd_out = open(pCmdLine->outputRedirect, O_WRONLY | O_CREAT | O_TRUNC, 0644);
                if (fd_out == -1) {
                    perror("output redirection failed");
                    _exit(1);
                }
                dup2(fd_out, STDOUT_FILENO);
                close(fd_out);
            }

            if (execvp(cmd, pCmdLine->arguments) == -1) {
                perror("Execution failed");
                _exit(1);
            }
        } else {
            if (debug_mode) {
                fprintf(stderr, "PID: %d\n", pid);
                fprintf(stderr, "Executing program: %s\n", cmd);
                if (pCmdLine->blocking) {
                    fprintf(stderr, "Mode: Foreground\n");
                } else {
                    fprintf(stderr, "Mode: Background\n");
                }
            }

            addProcess(&process_list, pCmdLine, pid);

            if (pCmdLine->blocking) {
                waitpid(pid, NULL, 0);
            }
        }
    }
}

int main(int argc, char** argv) {
    char path[PATH_MAX];
    char input[2048];
    cmdLine* pCmdL = NULL;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-d") == 0) {
            debug_mode = 1;
        }
    }

    while (1) {
        if (getcwd(path, PATH_MAX) == NULL) {
            perror("getcwd error");
            exit(1);
        }
        printf("%s: ", path);
        if (fgets(input, 2048, stdin) == NULL) {
            break;
        }

        if (strcmp(input, "\n") == 0) {
            continue;
        }

        pCmdL = parseCmdLines(input);

        if (pCmdL == NULL) {
            continue;
        }

        if (pCmdL->next != NULL && pCmdL->outputRedirect != NULL) {
            fprintf(stderr, "Error: Invalid output redirection on the left side of a pipe.\n");
            freeCmdLines(pCmdL);
            continue;
        }

        if (pCmdL->next != NULL && pCmdL->next->inputRedirect != NULL) {
            fprintf(stderr, "Error: Invalid input redirection on the right side of a pipe.\n");
            freeCmdLines(pCmdL);
            continue;
        }

        if (strcmp(pCmdL->arguments[0], "quit") == 0) {
            freeCmdLines(pCmdL);
            break;
        }

        int is_builtin = 0;
        if (strcmp(pCmdL->arguments[0], "cd") == 0 ||
            strcmp(pCmdL->arguments[0], "stop") == 0 ||
            strcmp(pCmdL->arguments[0], "wakeup") == 0 ||
            strcmp(pCmdL->arguments[0], "ice") == 0 ||
            strcmp(pCmdL->arguments[0], "nuke") == 0 ||
            strcmp(pCmdL->arguments[0], "procs") == 0) {
            is_builtin = 1;
        }

        execute(pCmdL);

        if (is_builtin) {
            freeCmdLines(pCmdL);
        }
    }

    freeProcessList(process_list);
    puts("\n");
    return 0;
}