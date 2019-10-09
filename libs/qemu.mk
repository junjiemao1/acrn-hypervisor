
ifdef QEMU

ZEPHYR_DIR := ../../zephyrproject
LINUX_DIR := ../../linux
UT_DIR := ../../acrn-unit-test

ZEPHYR_SAMPLE ?= hello_world
LINUX_ROOTFS ?= clear-31090-kvm.img
UT_CASE ?= xsave
UT_CASE_2 ?= tsc_adjust

ZEPHYR_BINARY := $(ZEPHYR_DIR)/zephyr/samples/$(ZEPHYR_SAMPLE)/build/zephyr/zephyr.bin
LINUX_BZIMAGE_BINARY := $(LINUX_DIR)/build/arch/x86/boot/bzImage
LINUX_ROOTFS_IMAGE := $(LINUX_DIR)/$(LINUX_ROOTFS)
UT_BINARY := $(UT_DIR)/guest/x86/$(UT_CASE).bzimage
UT_BINARY_2 := $(UT_DIR)/guest/x86/$(UT_CASE_2).bzimage

GRUBISO := ./grub_iso
QEMUEXE	:= qemu-system-x86_64

QEMU_CPU := max,level=21,invtsc
QEMUOPTS += -machine q35,kernel_irqchip=split,accel=kvm -cpu $(QEMU_CPU)
QEMUOPTS += -m 4G -smp cpus=8,cores=4,threads=2 -enable-kvm
QEMUOPTS += -device isa-debug-exit -device intel-iommu,intremap=on,caching-mode=on,device-iotlb=on
QEMUOPTS += -debugcon file:/dev/stdout
QEMUOPTS += -serial mon:stdio -display none

ifndef UT
$(HV_OBJDIR)/$(HV_FILE).iso: all $(ZEPHYR_BINARY) $(LINUX_BZIMAGE_BINARY)
	$(GRUBISO) -t 1 $(HV_OBJDIR)/$(HV_FILE).iso $(HV_OBJDIR)/$(HV_FILE).32.out \
		$(ZEPHYR_BINARY):zephyr \
		$(LINUX_BZIMAGE_BINARY):linux
else
$(UT_DIR)/guest/x86/%.bzimage:
	make -C $(UT_DIR)/guest x86/$*.bzimage

$(HV_OBJDIR)/$(HV_FILE).iso: all $(UT_BINARY) $(UT_BINARY_2)
	$(GRUBISO) -t 1 $(HV_OBJDIR)/$(HV_FILE).iso $(HV_OBJDIR)/$(HV_FILE).32.out \
		$(UT_BINARY):unit_test_1 \
		$(UT_BINARY_2):unit_test_2
endif

.PHONY: qemu
qemu: $(HV_OBJDIR)/$(HV_FILE).iso
	$(QEMUEXE) $(QEMUOPTS) -cdrom $(HV_OBJDIR)/$(HV_FILE).iso || true

.PHONY: qemu-debug
qemu-debug: $(HV_OBJDIR)/$(HV_FILE).iso
	$(QEMUEXE) $(QEMUOPTS) -s -S -cdrom $(HV_OBJDIR)/$(HV_FILE).iso || true

.PHONY: gdb
gdb:
	gdb $(HV_OBJDIR)/$(HV_FILE).out -ex "target remote localhost:1234"

endif
