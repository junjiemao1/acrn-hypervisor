/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <vm.h>
#include <e820.h>
#include <zeropage.h>
#include <boot_context.h>
#include <ept.h>
#include <mmu.h>
#include <multiboot.h>
#include <errno.h>
#include <sprintf.h>
#include <logmsg.h>
#include <unit_test.h>
#include <multiboot.h>
#include <boot_context.h>

static struct vm_io_range testdev_range = {
	.flags = IO_ATTR_RW,
	.base = 0xf4U,
	.len = 4U,
};

static bool
testdev_io_read(__unused struct acrn_vm *vm, __unused struct acrn_vcpu *vcpu, __unused uint16_t port, __unused size_t size)
{
	return true;
}

static bool
testdev_io_write(struct acrn_vm *vm, __unused uint16_t port, __unused size_t size, __unused uint32_t val)
{
	uint16_t i;
	struct acrn_vcpu *vcpu = NULL;

	foreach_vcpu(i, vm, vcpu) {
		pause_vcpu(vcpu, VCPU_PAUSED);
	}

	return true;
}

static void prepare_bsp_gdt(struct acrn_vm *vm)
{
	size_t gdt_len;
	uint64_t gdt_base_hpa;

	gdt_base_hpa = gpa2hpa(vm, boot_context.gdt.base);
	if (boot_context.gdt.base != gdt_base_hpa) {
		gdt_len = ((size_t)boot_context.gdt.limit + 1U) / sizeof(uint8_t);
		(void)copy_to_gpa(vm, hpa2hva(boot_context.gdt.base), boot_context.gdt.base, gdt_len);
	}

	return;
}

static uint64_t create_multiboot_info(struct acrn_vm *vm)
{
	struct multiboot_info *boot_info;
	char *cmdline;
	struct sw_kernel_info *sw_kernel = &(vm->sw.kernel_info);
	struct sw_module_info *bootargs_info = &(vm->sw.bootargs_info);
	struct acrn_vm_config *vm_config = get_vm_config(vm->vm_id);
	uint64_t gpa_boot_info, gpa_cmdline;

	gpa_boot_info = (uint64_t)sw_kernel->kernel_load_addr - MEM_4K;
	gpa_cmdline = gpa_boot_info + MEM_2K;
	cmdline = (char *)gpa2hva(vm, gpa_cmdline);

	stac();
	boot_info = (struct multiboot_info *)gpa2hva(vm, gpa_boot_info);
	boot_info->mi_flags = MULTIBOOT_INFO_HAS_MEMORY | MULTIBOOT_INFO_HAS_CMDLINE | MULTIBOOT_INFO_HAS_MODS;
	boot_info->mi_mem_lower = 0U;
	boot_info->mi_mem_upper = vm_config->memory.size / MEM_1K;
	boot_info->mi_cmdline = (uint32_t)gpa_cmdline;
	strncpy_s(cmdline, MEM_2K, bootargs_info->src_addr, MEM_2K);
	boot_info->mi_mods_count = 0U;
	clac();

	return gpa_boot_info;
}

int32_t unit_test_sw_loader(struct acrn_vm *vm)
{
	int32_t ret = 0;
	uint32_t i;
	uint32_t kernel_entry_offset = 0U;
	struct sw_kernel_info *sw_kernel = &(vm->sw.kernel_info);
	struct acrn_vcpu *vcpu = vcpu_from_vid(vm, BOOT_CPU_ID);

	pr_dbg("Loading guest to run-time location");

	prepare_bsp_gdt(vm);
	set_vcpu_regs(vcpu, &boot_context);

	/* Search for the multiboot header magic. The entry point is right after
	 * the multiboot header which is 12-byte long.
	 */

	/* The multiboot header must reside in the first 8K bytes of the image
	 * and 32-bit aligned. */
	stac();
	for (i = 0U; i < 8 * MEM_1K; i += 4U) {
		uint32_t *magic = (uint32_t *)(sw_kernel->kernel_src_addr + i);
		if (*magic == MULTIBOOT_HEADER_MAGIC &&
		    magic[0] + magic[1] + magic[2] == 0U) {
			kernel_entry_offset = i + 12U;
			break;
		}
	}
	clac();
	if (kernel_entry_offset == 0U) {
		panic("Unrecognized image format: no multiboot header detected.");
	}

	sw_kernel->kernel_entry_addr =
		(void *)((uint64_t)sw_kernel->kernel_load_addr
			+ kernel_entry_offset);
	if (is_vcpu_bsp(vcpu)) {
		/* Set VCPU entry point to kernel entry */
		vcpu_set_rip(vcpu, (uint64_t)sw_kernel->kernel_entry_addr);
		pr_info("%s, VM %hu VCPU %hu Entry: 0x%016llx ",
			__func__, vm->vm_id, vcpu->vcpu_id,
			sw_kernel->kernel_entry_addr);
	}

	printf("load: %x\n", sw_kernel->kernel_load_addr);
	printf("entry: %x\n", sw_kernel->kernel_entry_addr);

	stac();
	(void)copy_to_gpa(vm, sw_kernel->kernel_src_addr,
		(uint64_t)sw_kernel->kernel_load_addr, sw_kernel->kernel_size);
	clac();

	/* Documentation states:
	 *     eax = MULTIBOOT_INFO_MAGIC
	 *     ebx = physical address of multiboot info
	 */
	for (i = 0U; i < NUM_GPRS; i++) {
		vcpu_set_gpreg(vcpu, i, 0UL);
	}
	vcpu_set_gpreg(vcpu, CPU_REG_RAX, MULTIBOOT_INFO_MAGIC);
	vcpu_set_gpreg(vcpu, CPU_REG_RBX, create_multiboot_info(vm));

	register_pio_emulation_handler(vm, TESTDEV_PIO_IDX, &testdev_range, testdev_io_read, testdev_io_write);

	return ret;
}
