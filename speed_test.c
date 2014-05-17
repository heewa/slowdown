/* Simply increment a counter at a fixed frequency, and compare actual
 * increments with expected. If there aren't as many, that's how much
 * slower it's running than it should be.
 */
#include <stdio.h>
#include <sys/time.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>

#define PERIOD_SEC (1.0)
#define FREQ_SEC (0.001)
const double MAX_INCR_PER_SEC = 1.0 / FREQ_SEC;

int counter;
struct timeval last_period;


double elapsed_sec(struct timeval* end, struct timeval* start) {
    double sec = end->tv_sec - start->tv_sec;
    double usec = end->tv_usec - start->tv_usec;
    return sec + usec / (1000 * 1000);
}


void interval(int signal) {
    struct timeval now;
    double elapsed;

    ++counter;

    gettimeofday(&now, NULL);
    elapsed = elapsed_sec(&now, &last_period);

    if (elapsed >= PERIOD_SEC) {
        double expected_increments = MAX_INCR_PER_SEC * elapsed;
        double speed_perc = 100.0 * counter / expected_increments;

        printf("%6.2f%%, %5.2f sec, %4d incr, %6.1f expected incr\n",
               speed_perc,
               elapsed, counter, expected_increments);

        counter = 0;
        last_period = now;
    }
}


int main(int argc, char** argv) {
    struct itimerval timer_interval;
    struct sigaction action;

    // Clear counter and time-tracker.
    counter = 0;
    gettimeofday(&last_period, NULL);

    // Set up timing.
    timer_interval.it_interval.tv_sec = (int) FREQ_SEC;
    timer_interval.it_interval.tv_usec =
        (FREQ_SEC - ((int) FREQ_SEC)) * (1000 * 1000);
    timer_interval.it_value = timer_interval.it_interval;

    // Set up handler for alarm (timer).
    memset(&action, 0, sizeof(struct sigaction));
    action.sa_handler = &interval;
    if (sigaction(SIGALRM, &action, NULL) != 0) {
        fprintf(stderr, "Failed to set up timer handler.\n");
        return -1;
    }

    if (setitimer(ITIMER_REAL, &timer_interval, NULL) != 0) {
        fprintf(stderr, "Failed to start timer.\n");
        return -1;
    }

    // meh?
    while (1) {
        sleep(60);
    }

    return 0;
}
