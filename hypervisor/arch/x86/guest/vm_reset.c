/*
 * Copyright (C) 2019 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <vm.h>
#include <io.h>
#include <logmsg.h>
#include <per_cpu.h>
#include <vm_reset.h>
#include <default_acpi_info.h>
#include <platform_acpi_info.h>

/**
 * @pre vm != NULL
 */
void triple_fault_shutdown_vm(struct acrn_vm *vm)
{
	struct acrn_vcpu *vcpu = vcpu_from_vid(vm, BOOT_CPU_ID);

	/* Either SOS or pre-launched VMs */
	pause_vm(vm);

	per_cpu(shutdown_vm_id, vcpu->pcpu_id) = vm->vm_id;
	make_shutdown_vm_request(vcpu->pcpu_id);
}

/*
 * Reset Control register at I/O port 0xcf9.
 *     Bit 1 - 0: "soft" reset. Force processor begin execution at power-on reset vector.
 *	     1: "hard" reset. e.g. assert PLTRST# (if implemented) to do a host reset.
 *     Bit 2 - initiates a system reset when it transitions from 0 to 1.
 *     Bit 3 - 1: full reset (aka code reset), SLP_S3#/4#/5# or similar pins are asserted for full power cycle.
 *	     0: will be dropped if system in S3/S4/S5.
 */

void shutdown_vm_from_idle(uint16_t pcpu_id)
{
	struct acrn_vm *vm = get_vm_from_vmid(per_cpu(shutdown_vm_id, pcpu_id));
	const struct acrn_vcpu *vcpu = vcpu_from_vid(vm, BOOT_CPU_ID);

	if (vcpu->pcpu_id == pcpu_id) {
		(void)shutdown_vm(vm);
	}
}
