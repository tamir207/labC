#include <stdio.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <signal.h>
#include <string.h>

void handler(int sig)
{
	if (sig == SIGTSTP) {
		printf("\nLooper handling SIGTSTP\n");
		signal(SIGTSTP, SIG_DFL);
	} else if (sig == SIGCONT) {
		printf("\nLooper handling SIGCONT\n");
		signal(SIGCONT, SIG_DFL);
	} else if (sig == SIGINT) {
		printf("\nLooper handling SIGINT\n");
		signal(SIGINT, SIG_DFL);
	}

	fflush(stdout);
	raise(sig);

	if (sig == SIGTSTP) {
		signal(SIGCONT, handler);
	} else if (sig == SIGCONT) {
		signal(SIGTSTP, handler);
	}
}

int main(int argc, char **argv)
{
	printf("Starting the program\n");

	signal(SIGINT, handler);
	signal(SIGTSTP, handler);
	signal(SIGCONT, handler);

	while (1) {
		sleep(1);
	}

	return 0;
}