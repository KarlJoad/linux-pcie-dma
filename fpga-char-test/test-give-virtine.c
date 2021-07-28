#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <errno.h>
#include <stdint.h>

#include "ioctls.h"

/* Invoke with test-give-virtine */
int main(int argc, char **argv) {
    if(argc > 1) {
        printf("Too number of arguments!\n");
        printf("Invoke with test-give-virtine\n");
        return EXIT_FAILURE;
    }

    // Heap alloc, so we can write address of pointer, and for lifetime.
    uint64_t *virtine_to_clean = malloc(sizeof(uint64_t));
    *virtine_to_clean = 0xdeadbeef;
    printf("Will write \"virtine\" addr: %p with size %zd\n", virtine_to_clean,
           sizeof(virtine_to_clean));

    int virtine_fd = open("/dev/virtine_fpga", O_RDWR | O_SYNC | O_DSYNC);
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

    long ioctl_ret_val = ioctl(virtine_fd, FPGA_CHAR_RING_DOORBELL);
    if(ioctl_ret_val >= 0) {
        printf("Doorbell has been rung!\n");
    } else {
        printf("Could not ring doorbell!\n");
        goto fail_exit;
    }

    return EXIT_SUCCESS;

fail_exit:
    close(virtine_fd);
    printf("Printing and returning errno: %d\n\n", errno);
    return EXIT_FAILURE;
}
