FPGA_CHAR_VERSION = 1.0
FPGA_CHAR_SITE = $(BR2_EXTERNAL_FPGA_CHAR_PATH)
FPGA_CHAR_SITE_METHOD = local
$(eval $(kernel-module))
$(eval $(generic-package))
