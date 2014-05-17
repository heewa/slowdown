/* Slow down a process by repeatedly pausing & unpausing it.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/time.h>
#include <signal.h>

// The length of the intervals to consider pausing the target.
const double TICK_FREQ_SEC = 0.010;

// PID of process to slow down.
int pid = 0;

// Value to compare random() to for pause_perc.
long int pause_comp = 0;

// True if last interval program was paused.
int was_paused = 0;


void interval(int signal) {
    int should_pause = random() < pause_comp;

    if (!was_paused && should_pause) {
        if (kill(pid, SIGTSTP) == 0) {
            was_paused = 1;
        }
    } else if (was_paused && !should_pause) {
        if (kill(pid, SIGCONT) == 0) {
            was_paused = 0;
        }
    }
}


void cleanup(int sig) {
    if (was_paused) {
        if (kill(pid, SIGCONT) != 0) {
            fprintf(stderr, "Failed to resume process before exiting.\n");
        } else {
            was_paused = 0;
        }
    }
    printf("\n");
    exit(0);
}


int main(int argc, char** argv) {
    char* end = NULL;
    int pause_perc = 50;
    struct itimerval timer_interval;
    struct sigaction interval_action;
    struct sigaction cleanup_action;

    if (argc < 2) {
        fprintf(stderr, "Run with the PID of the program to slow down.\n");
        return -1;
    }

    pid = (int) strtol(argv[1], &end, 10);
    if (*argv[1] == '\0' || !end || *end != '\0') {
        fprintf(stderr, "Unable to parse PID argument as int.\n");
        return -1;
    }

    if (argc >= 3) {
        pause_perc = (int) strtol(argv[2], &end, 10);
        if (*argv[2] == '\0' || !end || *end != '\0') {
            fprintf(stderr, "Unable to parse pause percent argument as int.\n");
            return -1;
        }
        if (pause_perc < 0 || pause_perc > 100) {
            fprintf(stderr, "Pause percent should be between 0 and 100.\n");
            return -1;
        }
    }
    pause_comp = (long int) (RAND_MAX * (1.0 - (double) pause_perc / 100.0));

    srandom(time(NULL));

    // Set up timing.
    timer_interval.it_interval.tv_sec = (int) TICK_FREQ_SEC;
    timer_interval.it_interval.tv_usec =
        (TICK_FREQ_SEC - ((int) TICK_FREQ_SEC)) * (1000 * 1000);
    timer_interval.it_value = timer_interval.it_interval;

    // Set up handler for alarm (timer).
    memset(&interval_action, 0, sizeof(struct sigaction));
    interval_action.sa_handler = &interval;
    if (sigaction(SIGALRM, &interval_action, NULL) != 0) {
        fprintf(stderr, "Failed to set up timer handler.\n");
        return -1;
    }

    // Set a timer to actually send that alarm signal.
    if (setitimer(ITIMER_REAL, &timer_interval, NULL) != 0) {
        fprintf(stderr, "Failed to start timer.\n");
        return -1;
    }

    // Set up handler for exiting, to resume target process.
    memset(&cleanup_action, 0, sizeof(struct sigaction));
    cleanup_action.sa_handler = &cleanup;
    if (sigaction(SIGINT, &cleanup_action, NULL) != 0) {
        fprintf(stderr, "Failed to set cleanup handler.\n");
        return -1;
    }

    while (1) {
        sleep(60);
    }

    return 0;
}
