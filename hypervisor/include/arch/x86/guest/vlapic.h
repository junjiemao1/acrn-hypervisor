/*-
 * Copyright (c) 2011 NetApp, Inc.
 * Copyright (c) 2017 Intel Corporation
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY NETAPP, INC ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL NETAPP, INC OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#ifndef VLAPIC_H
#define VLAPIC_H

#include <page.h>
#include <timer.h>
#include <apicreg.h>

/**
 * @file vlapic.h
 *
 * @brief public APIs for virtual LAPIC
 */

#define VLAPIC_MAXLVT_INDEX	APIC_LVT_CMCI

struct vlapic_pir_desc {
} __aligned(64);

struct vlapic_timer {
	struct hv_timer timer;
	uint32_t mode;
	uint32_t divisor_shift;
};

struct acrn_vlapic {
	/*
	 * Please keep 'apic_page' and 'pir_desc' be the first two fields in
	 * current structure, as below alignment restrictions are mandatory
	 * to support APICv features:
	 * - 'apic_page' MUST be 4KB aligned.
	 * - 'pir_desc' MUST be 64 bytes aligned.
	 */
	struct lapic_regs	apic_page;
	struct vlapic_pir_desc	pir_desc;

	struct acrn_vm		*vm;
	struct acrn_vcpu	*vcpu;

	struct vlapic_timer	vtimer;

	/*
	 * isrv: vector number for the highest priority bit that is set in the ISR
	 */
	uint32_t	isrv;

	uint64_t	msr_apicbase;

	/*
	 * Copies of some registers in the virtual APIC page. We do this for
	 * a couple of different reasons:
	 * - to be able to detect what changed (e.g. svr_last)
	 * - to maintain a coherent snapshot of the register (e.g. lvt_last)
	 */
	uint32_t	svr_last;
	uint32_t	lvt_last[VLAPIC_MAXLVT_INDEX + 1];
} __aligned(PAGE_SIZE);

/**
 * @brief virtual LAPIC
 *
 * @addtogroup acrn_vlapic ACRN vLAPIC
 * @{
 */

bool vlapic_inject_intr(struct acrn_vlapic *vlapic, bool guest_irq_enabled, bool injected);
bool vlapic_has_pending_delivery_intr(struct acrn_vcpu *vcpu);

uint64_t vlapic_get_apicbase(const struct acrn_vlapic *vlapic);
int32_t vlapic_set_apicbase(struct acrn_vlapic *vlapic, uint64_t new);
int32_t vlapic_x2apic_read(struct acrn_vcpu *vcpu, uint32_t msr, uint64_t *val);
int32_t vlapic_x2apic_write(struct acrn_vcpu *vcpu, uint32_t msr, uint64_t val);

uint32_t vlapic_get_apicid(const struct acrn_vlapic *vlapic);
int32_t vlapic_create(struct acrn_vcpu *vcpu);
/**
 * @pre vlapic->vm != NULL
 * @pre vlapic->vcpu->vcpu_id < CONFIG_MAX_VCPUS_PER_VM
 */
void vlapic_init(struct acrn_vlapic *vlapic);
void vlapic_reset(struct acrn_vlapic *vlapic);
uint64_t vlapic_apicv_get_apic_access_addr(void);
void vlapic_calc_dest(struct acrn_vm *vm, uint64_t *dmask, bool is_broadcast,
		uint32_t dest, bool phys, bool lowprio);
/**
 * @}
 */
/* End of acrn_vlapic */
#endif /* VLAPIC_H */
