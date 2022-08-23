obj-m=lkm_gpio_dev.o

KERNEL:=/var/aosp/lineage_kernel
KMOD_DIR:=$(shell pwd)

all:
	make ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- -C $(KERNEL) M=$(KMOD_DIR) modules
	rm lkm_gpio_dev.mod  lkm_gpio_dev.mod.c  lkm_gpio_dev.mod.o  lkm_gpio_dev.o  modules.order  Module.symvers
	rm ./.l*
	rm ./.M*
	rm ./.m*
