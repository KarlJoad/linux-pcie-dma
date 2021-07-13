#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <errno.h>
#include <stdint.h>

#include "ioctls.h"

enum ioctl_to_test {
    MAX_VIRTINES,
    CHANGE_BATCH_FACTOR,
    RING_DOORBELL,
};

/* Invoke with test-ioctls <ioctl-to-test>
 * dir is either read or write
 * offset is the memory address/register to read/write from/to
 * val can only be specified if writing. The value to write. */
int main(int argc, char **argv) {
    if(argc < 2) {
        printf("Incorrect number of arguments!\n");
        printf("Invoke with test-ioctls <ioctl-to-test>\n");
        printf("The ioctl to test should be a key word from the list below:\n");
        printf("\tmax_virtines - Fetch the maximum number of virtines each queue supports\n");
        printf("\tchange_batch_factor - Change batch factor before interrupt raised\n");
        printf("\tring_doorbell - Ring the doorbell, telling card to begin processing\n");
        return EXIT_FAILURE;
    }

    enum ioctl_to_test test;
    if(strcmp("max_virtines", argv[1]) == 0) {
        test = MAX_VIRTINES;
    }
    else if(strcmp("change_batch_factor", argv[1]) == 0) {
        test = CHANGE_BATCH_FACTOR;
    }
    else if(strcmp("ring_doorbell", argv[1]) == 0) {
        test = RING_DOORBELL;
    }
    else {
        printf("\"%s\" is an unsupported ioctl to test. Exiting!\n", argv[1]);
        return errno;
    }

    int virtine_fd = open("/dev/virtine_fpga", O_RDWR | O_SYNC | O_DSYNC);
    if(virtine_fd < 0) {
        printf("Could not open Virtine FPGA character device!\n");
        printf("Are you sure you loaded the fpga_char kernel module?\n");
        printf("errno value %d. errno is also the return value.\n", errno);
        return errno;
    }

    /* ioctl numbers are redefined for user-space in ioctls.h */
    long ioctl_ret_val;
    switch(test) {
    case MAX_VIRTINES: {
        unsigned long num_virtines = -1;
        ioctl_ret_val = ioctl(virtine_fd, FPGA_CHAR_GET_MAX_NUM_VIRTINES, &num_virtines);
        printf("Total supported virtines in each queue is %lu\n", num_virtines);
        break;
    }
    case CHANGE_BATCH_FACTOR: {
        if(argc != 3) {
            printf("Incorrect number of arguments passed for changing batch factor!\n");
            printf("Format; test-ioctls change_batch_factor <new_batch_factor>\n");
            goto fail_exit;
        }
        unsigned long new_batch_factor = strtoul(argv[2], NULL, 0);
        ioctl_ret_val = ioctl(virtine_fd, FPGA_CHAR_MODIFY_BATCH_FACTOR, new_batch_factor);
        if(ioctl_ret_val >= 0) {
            printf("Changed Batch factor. Verify that the operation was successful.\n");
        }
        break;
    }
    case RING_DOORBELL:
        ioctl_ret_val = ioctl(virtine_fd, FPGA_CHAR_RING_DOORBELL);
        if(ioctl_ret_val >= 0) {
            printf("Doorbell has been rung!\n");
        }
        break;
    default:
        printf("Unsupported ioctl test. Exiting!\n");
        return EXIT_FAILURE;
    }

    if(ioctl_ret_val < 0) {
        goto fail_exit;
    }

    return EXIT_SUCCESS;

fail_exit:
    close(virtine_fd);
    printf("Printing and returning errno: %d\n\n", errno);
    return EXIT_FAILURE;
}
