#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <errno.h>
#include <stdint.h>
#include <signal.h>

#include "ioctls.h"

static void signal_handler(int signum);

/* Heap alloc, so we can write address of pointer, and for lifetime.
 * Marked volatile to ensure value is actually updated during signal
 * handling. */
volatile uint64_t *virtine_to_clean;
int virtine_fd;

volatile sig_atomic_t have_received_signal = 0;

/* Invoke with test-give-virtine */
int main(int argc, char **argv) {
    if(argc > 1) {
        printf("Too many arguments!\n");
        printf("Invoke with test-give-virtine\n");
        return EXIT_FAILURE;
    }

    virtine_to_clean = malloc(sizeof(uint64_t));
    *virtine_to_clean = 0xdeadbeef;
    printf("Will write \"virtine\" addr: %p with size %zd\n", virtine_to_clean,
           sizeof(virtine_to_clean));
    printf("\"Virtine\" points to value: 0x%lx\n", *virtine_to_clean);

    virtine_fd = open("/dev/virtine_fpga", O_RDWR | O_SYNC | O_DSYNC);
    if(virtine_fd < 0) {
        printf("Could not open Virtine FPGA character device!\n");
        printf("Are you sure you loaded the fpga_char kernel module?\n");
        printf("errno value %d. errno is also the return value.\n", errno);
        return errno;
    }

    unsigned long offset = 0x8; // Location of RQ TAIL register
    int bytes_handled = pwrite(virtine_fd, &virtine_to_clean, sizeof(&virtine_to_clean), offset);

    if(bytes_handled < 0) {
        printf("WRITE FAILED! Address not in FPGA. Exiting!\n");
        goto fail_exit;
    }
    else {
        printf("Write \"virtine\" addr %p succeeded!\n", virtine_to_clean);
    }

    struct sigaction sig_act;
    sig_act.sa_handler = signal_handler,
    sigemptyset(&sig_act.sa_mask);
    sig_act.sa_flags = SA_RESTART; /* Restart functions if interrupted by handler */

    sigaction(SIGUSR1, &sig_act, NULL); /* NULL because we do not need to know previous sigaction */
    fcntl(virtine_fd, F_SETOWN, getpid()); /* Give char device the PID of this process */
    int oflags = fcntl(virtine_fd, F_GETFL); /* Retrieve the file's current flags */
    fcntl(virtine_fd, F_SETFL, oflags | FASYNC); /* Add this process as an async reader */

    long ioctl_ret_val = ioctl(virtine_fd, FPGA_CHAR_RING_DOORBELL);
    if(ioctl_ret_val >= 0) {
        printf("Doorbell has been rung!\n");
    } else {
        printf("Could not ring doorbell!\n");
        goto fail_exit;
    }

    printf("\"Virtine\" points to value: 0x%lx\n", *virtine_to_clean);

    sigset_t virtine_signal_set;
    sigaddset(&virtine_signal_set, SIGUSR1);
    sigsuspend(&virtine_signal_set);
    // Stop until signal is thrown and caught. pause returns when handler is done
    pause();

    if(have_received_signal) {
        printf("Received signal! Everything is done!\n");
    }
    else {
        printf("Signal NOT received! Failed!\n");
    }

    return EXIT_SUCCESS;

fail_exit:
    close(virtine_fd);
    printf("Printing and returning errno: %d\n\n", errno);
    return EXIT_FAILURE;
}

static void signal_handler(int signum) {
    long bytes_returned = ioctl(virtine_fd, FPGA_CHAR_FETCH_CLEAN_VIRTINES,
                                virtine_to_clean);
    have_received_signal = 1;
}
