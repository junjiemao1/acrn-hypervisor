/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef VMCS_H_
#define VMCS_H_

#define VM_SUCCESS	0
#define VM_FAIL		-1

#ifndef ASSEMBLER
#include <types.h>
#include <vmx.h>
#include <vcpu.h>

#define VMX_VMENTRY_FAIL		0x80000000U

#define VMX_SUPPORT_UNRESTRICTED_GUEST (1U<<5U)

void init_vmcs(struct acrn_vcpu *vcpu);

uint64_t vmx_rdmsr_pat(const struct acrn_vcpu *vcpu);
int32_t vmx_wrmsr_pat(struct acrn_vcpu *vcpu, uint64_t value);

void switch_apicv_mode_x2apic(struct acrn_vcpu *vcpu);

static inline enum vm_cpu_mode get_vcpu_mode(const struct acrn_vcpu *vcpu)
{
	return vcpu->arch.cpu_mode;
}

#endif /* ASSEMBLER */

#endif /* VMCS_H_ */
