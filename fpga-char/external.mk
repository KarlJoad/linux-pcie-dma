VIRTINE_FPGA_VERSION = 1.0
VIRTINE_FPGA_SITE = $(BR2_EXTERNAL_VIRTINE_FPGA_PATH)
VIRTINE_FPGA_SITE_METHOD = local

$(eval $(kernel-module))
$(eval $(generic-package))
