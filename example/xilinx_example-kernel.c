/**
 * \brief ...
 * \param zds_rf2pcie_device_p ...
 * \param zds_rf2pcie_ioctl_args_p ...
 * \return ...
 */
long
zds_rf2pcie_read_uint64_uint8(
    zds_rf2pcie_device_t * zds_rf2pcie_device_p,
    unsigned long zds_rf2pcie_ioctl_args_p)
{
    long return_value = RETURN_FAILURE;

    // Where are we?
    DEBUG_INFO("%s> Entering function %s()\n", ZDS_RF2PCIE_DRIVER_NAME, __FUNCTION__)
    DEBUG_INFO("%s> [a] zds_rf2pcie_device_p = 0x%016p\n", ZDS_RF2PCIE_DRIVER_NAME, zds_rf2pcie_device_p)
    DEBUG_INFO("%s> [a] zds_..._ioctl_args_p = 0x%016p\n", ZDS_RF2PCIE_DRIVER_NAME, (void *)zds_rf2pcie_ioctl_args_p)

    zds_rf2pcie_ioctl_pcie_memory_access_args_t zds_rf2pcie_ioctl_args;

    // Let's try to copy the arguments from the user space.
    return_value =
        copy_from_user(
            &zds_rf2pcie_ioctl_args,
            (zds_rf2pcie_ioctl_pcie_memory_access_args_t *)zds_rf2pcie_ioctl_args_p,
            sizeof(zds_rf2pcie_ioctl_pcie_memory_access_args_t));

    // Verify that the arguments were copied from the user space.
    // It should have returned zero.
    if (return_value != 0)
    {
        // It didn't!
        // Let's return with an error code.
        printk(KERN_ERR "%s>%s> ERROR: The arguments couldn't be copied from the user space!\n", ZDS_RF2PCIE_DRIVER_NAME, __FUNCTION__);
        return (-ENOMEM);
    }

    uint8_t  pcie_region_index   = zds_rf2pcie_ioctl_args.pcie_region_index.uint8;
    uint64_t pcie_address_offset = zds_rf2pcie_ioctl_args.pcie_address_offset.uint64;

    // What did we find?
    DEBUG_INFO("%s> [a] pcie_region_index    = 0x%02x\n",  ZDS_RF2PCIE_DRIVER_NAME, pcie_region_index)
    DEBUG_INFO("%s> [a] pcie_address_offset  = 0x%016p\n", ZDS_RF2PCIE_DRIVER_NAME, (void *)pcie_address_offset)

    // Check if pcie_region_index is valid.
    // It should be in the range [0:3], and
    // it should refer to a PCIe region whose length is non-zero.
    if ((pcie_region_index >= EN_PCIE_REGIONS_4) ||
        (zds_rf2pcie_device_p->pcie_regions[pcie_region_index].region_length == 0))
    {
        // It isn't!
        // Let's return with an error code.
        return (-EINVAL);
    }

    // Let's compute the virtual address.
    // Let's align it to the  8b boundary.
    void * pcie_address;
    pcie_address  = zds_rf2pcie_device_p->pcie_regions[pcie_region_index].virt_address_start;
    pcie_address += zds_rf2pcie_device_p->pcie_regions[pcie_region_index].region_mask & 0x3FFFFFFFFFFFFFFF & pcie_address_offset;

    // What did we find?
    DEBUG_INFO("%s> ... Let's read  @ virtual address 0x%016p\n", ZDS_RF2PCIE_DRIVER_NAME, pcie_address)

    zds_rf2pcie_ioctl_args.pcie_data.uint8   = readb(pcie_address);

    // What did we find?
    DEBUG_INFO("%s> [r] pcie_data            = 0x%02x\n", ZDS_RF2PCIE_DRIVER_NAME, zds_rf2pcie_ioctl_args.pcie_data.uint8 )

    // Let's try to copy the arguments to the user space.
    return_value =
        copy_to_user(
            (zds_rf2pcie_ioctl_pcie_memory_access_args_t *)zds_rf2pcie_ioctl_args_p,
            &zds_rf2pcie_ioctl_args,
            sizeof(zds_rf2pcie_ioctl_pcie_memory_access_args_t));

    // Verify that the arguments were copied to the user space.
    // It should have returned zero.
    if (return_value != 0)
    {
        // It didn't!
        // Let's return with an error code.
        printk(KERN_ERR "%s>%s> ERROR: The arguments couldn't be copied to the user space!\n", ZDS_RF2PCIE_DRIVER_NAME, __FUNCTION__);
        return (-ENOMEM);
    }

    // NO BLUE SCREEN!?!?!
    // HURRAH!
    return (RETURN_SUCCESS);
}
