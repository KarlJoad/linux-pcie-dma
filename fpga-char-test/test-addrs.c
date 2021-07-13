#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <stdint.h>

enum action {
    READ,
    WRITE,
};

/* Invoke with test-addrs <dir> <offset> [val]
 * dir is either read or write
 * offset is the memory address/register to read/write from/to
 * val can only be specified if writing. The value to write. */
int main(int argc, char **argv) {
    if(!((argc == 3) || // Read Case
         (argc == 4))) { // Write Case
        printf("Incorrect number of arguments!\n");
        printf("Invoke with test-addrs <dir> <offset> [val]\n");
        printf("dir is either read or write\n");
        printf("offset is the memory address/register to read/write from/to\n");
        printf("val can only be specified if writing. The value to write.\n");
        printf("Both offset and val can be specified in decimal, hex(0x...) or octal (0...)\n");
        return EXIT_FAILURE;
    }

    enum action dir;
    unsigned int val;
    if(strcmp("read", argv[1]) == 0) {
        dir = READ;
    }
    else if(strcmp("write", argv[1]) == 0) {
        dir = WRITE;
        val = strtoul(argv[3], NULL, 0); // 0 allows for decimal, octal, and hex
    }
    else {
        printf("Invalid direction. Choose one of either \"read\" or \"write\"\n");
        return EXIT_FAILURE;
    }

    unsigned long offset = strtoul(argv[2], NULL, 0); // 0 allows for decimal, octal, and hex

    int virtine_fd = open("/dev/virtine_fpga", O_RDWR | O_SYNC | O_DSYNC);
    if(virtine_fd < 0) {
        printf("Could not open Virtine FPGA character device!\n");
        printf("Are you sure you loaded the fpga_char kernel module?\n");
        printf("errno value %d. errno is also the return value.\n", errno);
        return errno;
    }

    int bytes_handled;
    switch(dir) {
    case READ:
        if(argc > 3) {
            printf("Too many arguments provided to read Virtine file!\n");
            printf("Make sure to include just the offset when specifyin a read.\n");
            goto fail_exit;
        }
        bytes_handled = read(virtine_fd, &val, sizeof(val));
        break;
    case WRITE:
        if(argc != 4) {
            printf("Value to write not specified. Exiting!\n");
            goto fail_exit;
        }
        printf("Writing %ld to virtine character device.\n", val);
        bytes_handled = write(virtine_fd, &val, sizeof(val));
        break;
    default:
        printf("Direction went wonky. No idea what happened. Exiting!\n");
        return EXIT_FAILURE;
    }

    if(bytes_handled < 0) {
        printf("%s FAILED! Exiting!\n", dir == READ ? "READ" : "WRITE");
        goto fail_exit;
    }
    else {
        printf("%s value %ld succeeded!\n\n", dir == READ ? "READ" : "WRITE", val);
    }

    return EXIT_SUCCESS;

fail_exit:
    close(virtine_fd);
    printf("Printing and returning errno: %d\n\n", errno);
    return EXIT_FAILURE;
}
