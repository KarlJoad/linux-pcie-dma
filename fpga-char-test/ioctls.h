/* All ioctl numbers must be manually fetched from the kernel module that
 * defines the number. As far as I know, there is no way for the module to
 * expose its ioctl numbers nicely. */
#define FPGA_CHAR_MODIFY_BATCH_FACTOR 0x80084630
#define FPGA_CHAR_GET_MAX_NUM_VIRTINES 0x40084631
#define FPGA_CHAR_RING_DOORBELL 0x4632
#define FPGA_CHAR_SET_SNAPSHOT 0x80084633

struct virtine_snapshot {
        unsigned long addr;
        unsigned long size;
};
