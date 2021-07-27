FPGA_CHAR_TEST_VERSION = 1.0
FPGA_CHAR_TEST_SITE = $(BR2_EXTERNAL_FPGA_CHAR_TEST_PATH)
FPGA_CHAR_TEST_SITE_METHOD = local

define FPGA_CHAR_TEST_BUILD_CMDS
	$(MAKE) $(TARGET_CONFIGURE_OPTS) -C $(@D) all
endef

define FPGA_CHAR_TEST_INSTALL_TARGET_CMDS
	$(INSTALL) -m 0755 -D $(@D)/test-addrs $(TARGET_DIR)/usr/bin/test-addrs
	$(INSTALL) -m 0755 -D $(@D)/test-ioctls $(TARGET_DIR)/usr/bin/test-ioctls
	$(INSTALL) -m 0755 -D $(@D)/test-give-virtine $(TARGET_DIR)/usr/bin/test-give-virtine
endef

$(eval $(generic-package))
