HELLO_WORLD_VERSION = 1.0
HELLO_WORLD_SITE = $(BR2_EXTERNAL_HELLO_WORLD_PATH)
HELLO_WORLD_SITE_METHOD = local
$(eval $(kernel-module))
$(eval $(generic-package))
