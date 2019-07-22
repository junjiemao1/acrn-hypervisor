/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <types.h>
#include <vm.h>
#include <vcpu.h>
#include <io_req.h>

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

void
register_testdev(struct acrn_vm *vm)
{
	/* Hack the CF9 handler */
	register_pio_emulation_handler(vm, CF9_PIO_IDX, &testdev_range, testdev_io_read, testdev_io_write);
}
