#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <stdint.h>
#include <stdbool.h>
#include <sys/prctl.h>
#include <string.h>
#include <sys/wait.h>

// What I learned:
//
// Fork and buffered text:
// =======================
// fork() copies the process’s memory, including any text still waiting in stdout’s buffer.
// so if we just fork(), the parent AND child would contain the same text in stdout’s buffer,
// and both may flush it later, leading to DUPLICATE OUTPUT!!!
//
// also, apparently text is flushed by \n...
//

typedef enum {
	FLAG_HELP = 1 << 0,
	FLAG_SET_CHILD_SUBREAPER = 1 << 1,
	FLAG_FORCE_ZOMBIE = 1 << 2
} FLAGS;

int main(int argc, char *argv[]) {
	int res, enabled_flags;
	pid_t pid, pid2;
	int status;
	pid_t reaped;
	bool has_matched;

	enabled_flags = 0;

	has_matched = false;

	if (argc > 1) {
		for (int i = 1; i < argc; i++) {
			if (strcmp(argv[i], "use_child_subreaper") == 0){
				enabled_flags |= FLAG_SET_CHILD_SUBREAPER;
				continue;
			}
			if (strcmp(argv[i], "allow_zombies") == 0){
				enabled_flags |= FLAG_FORCE_ZOMBIE;
				continue;
			}
			if (strcmp(argv[i], "help") == 0){
				enabled_flags |= FLAG_HELP;
				continue;
			}

			fprintf(stderr, "I do not understand: %s\n", argv[i]);
			exit(EXIT_FAILURE);
		}

	}

	if (enabled_flags & FLAG_HELP) {
		fprintf(stdout, "Usage: %s <flags>\n", argv[0]);
		exit(EXIT_SUCCESS);
	}


	if (enabled_flags & FLAG_SET_CHILD_SUBREAPER) {
		printf("[parent] setting PR_SET_CHILD_SUBREADER(1,0,0,0)\n");
		if (prctl(PR_SET_CHILD_SUBREAPER, 1, 0, 0, 0) == -1) {
			perror("[parent] prctl");
			exit(EXIT_FAILURE);
		}
	}

	fflush(stdout);
	pid = fork();
	if (pid == -1) {
		perror("[parent] fork");
		exit(EXIT_FAILURE);
	}
	if (pid == 0) {
		pid_t curpid;
		curpid = getpid();
		printf("[child 1] I am child 1 with pid: %jd\n", (intmax_t) curpid);

		fflush(stdout);
		pid2 = fork();

		if (pid2 == -1) {
			perror("[child 1] fork");
			exit(EXIT_FAILURE);
		}
		if (pid2 == 0) {
			printf("[child 2] I am child 2 with pid: %jd, parent: %jd\n",
			       (intmax_t) getpid(),
			       (intmax_t) getppid());

			sleep(5);

			printf("[child 2] 5 seconds later, my parent is now: %jd\n",
			       (intmax_t) getppid());

			sleep(15);

			
			exit(EXIT_SUCCESS);
		}

		printf("[child 1] Child 1 is exiting, goodbye!\n");
		exit(EXIT_SUCCESS);
	}

	printf("[parent] I am the parent. My pid is %jd. My child's is PID %jd\n", (intmax_t) getpid(), (intmax_t) pid);
	
	if (enabled_flags & FLAG_SET_CHILD_SUBREAPER) {
		printf("[parent] child subreaper has been set. The grandparent process must remain alive to handle child 2!\n");

		if (enabled_flags & FLAG_FORCE_ZOMBIE) {
			printf("[parent] Force zombie flag is set, parent will not await child!\n");
			printf("[parent] Going to sleep for 60 seconds\n");
			printf("[parent] Run \"ps -eo stat,pid,ppid,comm | grep -e '^[zZ]'\" and you should seem a zombie for child 1 and then child 2\n");
			sleep(60);
		} else {
			while ((reaped = wait(&status)) > 0) {
				printf("[parent] Parent reaped PID %jd\n", (intmax_t) reaped);
				if (WIFEXITED(status)) {
					printf("[parent] PID %jd exited with status %d\n",
					       (intmax_t) reaped,
					       WEXITSTATUS(status));
				} else if (WIFSIGNALED(status)) {
					printf("[parent] PID %jd killed by signal %d\n",
					       (intmax_t) reaped,
					       WTERMSIG(status));
				}
			}
		}

	} else {
		puts("[parent] Parent exiting.");
		exit(EXIT_SUCCESS);
	}
}
