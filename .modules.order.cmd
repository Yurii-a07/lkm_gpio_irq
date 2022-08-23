cmd_/var/aosp/mcross/lkm_gpio_dev/modules.order := {   echo /var/aosp/mcross/lkm_gpio_dev/lkm_gpio_dev.ko; :; } | awk '!x[$$0]++' - > /var/aosp/mcross/lkm_gpio_dev/modules.order
