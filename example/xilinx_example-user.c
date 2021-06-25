/** \brief Reads an unsigned integer in a BitField of a 8-bit data in PCI-Express memory address space.
 * ! \param[in] pcieRegionIndex The index of the PCI-Express region.
 * ! \param[in] pcieAddressOffset The address offset within the PCI-Express region.
 * ! \param[in] pcieBitFieldMask The BitField mask specifying the BitField within the data.
 * ! \return The unsigned integer read.
 */
uint8_t ReadUInt8(const uint8_t  pcieRegionIndex, const uint64_t pcieAddressOffset,
                  const uint8_t  pcieBitFieldMask) override final {
        IoctlPCIeMemoryAccessArgs ioctlPCIeMemoryAccessArgs;
        ioctlPCIeMemoryAccessArgs.PCIeRegionIndex.UInt8    = static_cast<uint8_t> (pcieRegionIndex);
        ioctlPCIeMemoryAccessArgs.PCIeAddressOffset.UInt64 = static_cast<uint64_t>(pcieAddressOffset);
        ioctlPCIeMemoryAccessArgs.PCIeDataMask.UInt8       = static_cast<uint8_t> (pcieBitFieldMask);

        int returnValue;

        // Let's call the character driver,
        // and hope it does what we expect.
        returnValue = ioctl(
                this->deviceDescriptor_,
                ZDS_RF2PCIE_READ_UINT64_UINT8_UINT8,
                &ioctlPCIeMemoryAccessArgs);

        // Let's check if the call to ioctl() was successful,
        // and take the appropriate actions if it wasn't.
        this->CheckIfIOCTLWasSuccessful(returnValue);

        // HURRAH!
        return (static_cast<uint8_t> (ioctlPCIeMemoryAccessArgs.PCIeData.UInt8));
}
