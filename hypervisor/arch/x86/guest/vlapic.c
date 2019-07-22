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

#define pr_prefix		"vlapic: "

#include <types.h>
#include <errno.h>
#include <bits.h>
#include <atomic.h>
#include <per_cpu.h>
#include <pgtable.h>
#include <lapic.h>
#include <vmcs.h>
#include <vlapic.h>
#include <ptdev.h>
#include <vmx.h>
#include <vm.h>
#include <ept.h>
#include <trace.h>
#include <logmsg.h>
#include "vlapic_priv.h"

#define	APICBASE_BSP		0x00000100UL
#define	APICBASE_X2APIC		0x00000400U
#define APICBASE_XAPIC		0x00000800U
#define APICBASE_LAPIC_MODE	(APICBASE_XAPIC | APICBASE_X2APIC)
#define	APICBASE_ENABLED	0x00000800UL
#define LOGICAL_ID_MASK		0xFU
#define CLUSTER_ID_MASK		0xFFFF0U

#define ACRN_DBG_LAPIC		6U

static struct acrn_vlapic *
vm_lapic_from_vcpu_id(struct acrn_vm *vm, uint16_t vcpu_id)
{
	struct acrn_vcpu *vcpu;

	vcpu = vcpu_from_vid(vm, vcpu_id);

	return vcpu_vlapic(vcpu);
}

static uint16_t vm_apicid2vcpu_id(struct acrn_vm *vm, uint32_t lapicid)
{
	uint16_t i;
	struct acrn_vcpu *vcpu;
	uint16_t cpu_id = INVALID_CPU_ID;

	foreach_vcpu(i, vm, vcpu) {
		const struct acrn_vlapic *vlapic = vcpu_vlapic(vcpu);
		if (vlapic_get_apicid(vlapic) == lapicid) {
			cpu_id = vcpu->vcpu_id;
			break;
		}
	}

	if (cpu_id == INVALID_CPU_ID) {
		pr_err("%s: bad lapicid %lu", __func__, lapicid);
	}

	return cpu_id;

}

/*
 * @pre vlapic != NULL
 */
uint32_t
vlapic_get_apicid(const struct acrn_vlapic *vlapic)
{
	uint32_t apicid;
	apicid = vlapic->apic_page.id.v;

	return apicid;
}

static inline uint32_t
vlapic_build_id(const struct acrn_vlapic *vlapic)
{
	const struct acrn_vcpu *vcpu = vlapic->vcpu;
	uint32_t vlapic_id, lapic_regs_id;

	vlapic_id = (uint32_t)vcpu->vcpu_id;

	lapic_regs_id = vlapic_id;

	dev_dbg(ACRN_DBG_LAPIC, "vlapic APIC PAGE ID : 0x%08x", lapic_regs_id);

	return lapic_regs_id;
}

static inline void vlapic_build_x2apic_id(struct acrn_vlapic *vlapic)
{
	struct lapic_regs *lapic;
	uint32_t logical_id, cluster_id;

	lapic = &(vlapic->apic_page);
	lapic->id.v = vlapic_build_id(vlapic);
	logical_id = lapic->id.v & LOGICAL_ID_MASK;
	cluster_id = (lapic->id.v & CLUSTER_ID_MASK) >> 4U;
	lapic->ldr.v = (cluster_id << 16U) | (1U << logical_id);
}

static inline void set_dest_mask_phys(struct acrn_vm *vm, uint64_t *dmask, uint32_t dest)
{
	uint16_t vcpu_id;

	vcpu_id = vm_apicid2vcpu_id(vm, dest);
	if (vcpu_id < vm->hw.created_vcpus) {
		bitmap_set_nolock(vcpu_id, dmask);
	}
}

/*
 * This function tells if a vlapic belongs to the destination.
 * If yes, return true, else reture false.
 *
 * @pre vlapic != NULL
 */
static inline bool is_dest_field_matched(const struct acrn_vlapic *vlapic, uint32_t dest)
{
	uint32_t logical_id, cluster_id, dest_logical_id, dest_cluster_id;
	uint32_t ldr = vlapic->apic_page.ldr.v;
	bool ret = false;

	logical_id = ldr & 0xFFFFU;
	cluster_id = (ldr >> 16U) & 0xFFFFU;
	dest_logical_id = dest & 0xFFFFU;
	dest_cluster_id = (dest >> 16U) & 0xFFFFU;
	if ((cluster_id == dest_cluster_id) && ((logical_id & dest_logical_id) != 0U)) {
		ret = true;
	}

	return ret;
}

/*
 * This function populates 'dmask' with the set of vcpus that match the
 * addressing specified by the (dest, phys, lowprio) tuple.
 */
void
vlapic_calc_dest(struct acrn_vm *vm, uint64_t *dmask, bool is_broadcast,
		uint32_t dest, bool phys, bool lowprio)
{
	struct acrn_vlapic *vlapic, *lowprio_dest = NULL;
	struct acrn_vcpu *vcpu;
	uint16_t vcpu_id;

	*dmask = 0UL;
	if (is_broadcast) {
		/* Broadcast in both logical and physical modes. */
		*dmask = vm_active_cpus(vm);
	} else if (phys) {
		/* Physical mode: "dest" is local APIC ID. */
		set_dest_mask_phys(vm, dmask, dest);
	} else {
		/*
		 * Logical mode: "dest" is message destination addr
		 * to be compared with the logical APIC ID in LDR.
		 */
		foreach_vcpu(vcpu_id, vm, vcpu) {
			vlapic = vm_lapic_from_vcpu_id(vm, vcpu_id);
			if (!is_dest_field_matched(vlapic, dest)) {
				continue;
			}

			if (lowprio) {
				/*
				 * for lowprio delivery mode, the lowest-priority one
				 * among all "dest" matched processors accepts the intr.
				 */
				if (lowprio_dest == NULL) {
					lowprio_dest = vlapic;
				} else if (lowprio_dest->apic_page.ppr.v > vlapic->apic_page.ppr.v) {
					lowprio_dest = vlapic;
				} else {
					/* No other state currently, do nothing */
				}
			} else {
				bitmap_set_nolock(vcpu_id, dmask);
			}
		}

		if (lowprio && (lowprio_dest != NULL)) {
			bitmap_set_nolock(lowprio_dest->vcpu->vcpu_id, dmask);
		}
	}
}

static void
vlapic_process_init_sipi(struct acrn_vcpu* target_vcpu, uint32_t mode, uint32_t icr_low)
{
	if (mode == APIC_DELMODE_INIT) {
		if ((icr_low & APIC_LEVEL_MASK) != APIC_LEVEL_DEASSERT) {

			dev_dbg(ACRN_DBG_LAPIC,
				"Sending INIT to %hu",
				target_vcpu->vcpu_id);

			/* put target vcpu to INIT state and wait for SIPI */
			pause_vcpu(target_vcpu, VCPU_PAUSED);
			reset_vcpu(target_vcpu);
			/* new cpu model only need one SIPI to kick AP run,
			 * the second SIPI will be ignored as it move out of
			 * wait-for-SIPI state.
			*/
			target_vcpu->arch.nr_sipi = 1U;
		}
	} else if (mode == APIC_DELMODE_STARTUP) {
		/* Ignore SIPIs in any state other than wait-for-SIPI */
		if ((target_vcpu->state == VCPU_INIT) &&
			(target_vcpu->arch.nr_sipi != 0U)) {

			dev_dbg(ACRN_DBG_LAPIC,
				"Sending SIPI to %hu with vector %u",
				 target_vcpu->vcpu_id,
				(icr_low & APIC_VECTOR_MASK));

			target_vcpu->arch.nr_sipi--;
			if (target_vcpu->arch.nr_sipi <= 0U) {

				pr_err("Start Secondary VCPU%hu for VM[%d]...",
					target_vcpu->vcpu_id,
					target_vcpu->vm->vm_id);
				set_ap_entry(target_vcpu, (icr_low & APIC_VECTOR_MASK) << 12U);
				schedule_vcpu(target_vcpu);
			}
		}
	} else {
		/* No other state currently, do nothing */
	}
	return;
}

static int32_t vlapic_read(struct acrn_vlapic *vlapic, uint32_t offset_arg, uint64_t *data)
{
	int32_t ret = 0;
	struct lapic_regs *lapic = &(vlapic->apic_page);
	uint32_t i;
	uint32_t offset = offset_arg;
	*data = 0ULL;

	if (offset > sizeof(*lapic)) {
		ret = -EACCES;
	} else {

		offset &= ~0x3UL;
		switch (offset) {
		case APIC_OFFSET_ID:
			*data = lapic->id.v;
			break;
		case APIC_OFFSET_LDR:
			*data = lapic->ldr.v;
			break;
		default:
			ret = -EACCES;
			break;
		}
	}

	dev_dbg(ACRN_DBG_LAPIC, "vlapic read offset %#x, data %#llx", offset, *data);
	return ret;
}

void
vlapic_reset(struct acrn_vlapic *vlapic)
{
	uint32_t i;
	struct lapic_regs *lapic;

	/*
	 * Upon reset, vlapic is set to xAPIC mode.
	 */
	vlapic->msr_apicbase = DEFAULT_APIC_BASE | APICBASE_ENABLED;

	if (vlapic->vcpu->vcpu_id == BOOT_CPU_ID) {
		vlapic->msr_apicbase |= APICBASE_BSP;
	}

	lapic = &(vlapic->apic_page);
	(void)memset((void *)lapic, 0U, sizeof(struct lapic_regs));

	lapic->id.v = vlapic_build_id(vlapic);
}

/**
 * @pre vlapic->vm != NULL
 * @pre vlapic->vcpu->vcpu_id < CONFIG_MAX_VCPUS_PER_VM
 */
void
vlapic_init(struct acrn_vlapic *vlapic)
{

	vlapic_reset(vlapic);
}

uint64_t vlapic_get_apicbase(const struct acrn_vlapic *vlapic)
{
	return vlapic->msr_apicbase;
}

int32_t vlapic_set_apicbase(struct acrn_vlapic *vlapic, uint64_t new)
{
	int32_t ret = 0;
	uint64_t changed;
	bool change_in_vlapic_mode = false;
	struct acrn_vcpu *vcpu = vlapic->vcpu;

	if (vlapic->msr_apicbase != new) {
		changed = vlapic->msr_apicbase ^ new;
		change_in_vlapic_mode = ((changed & APICBASE_LAPIC_MODE) != 0U);

		/*
		 * TODO: Logic to check for change in Reserved Bits and Inject GP
		 */

		/*
		 * Logic to check for change in Bits 11:10 for vLAPIC mode switch
		 */
		if (change_in_vlapic_mode) {
			if ((new & APICBASE_LAPIC_MODE) ==
						(APICBASE_XAPIC | APICBASE_X2APIC)) {
				vlapic->msr_apicbase = new;
				vlapic_build_x2apic_id(vlapic);
				switch_apicv_mode_x2apic(vlapic->vcpu);
				update_vm_vlapic_state(vcpu->vm);
			} else {
				/*
				 * TODO: Logic to check for Invalid transitions, Invalid State
				 * and mode switch according to SDM 10.12.5
				 * Fig. 10-27
				 */
			}
		}

		/*
		 * TODO: Logic to check for change in Bits 35:12 and Bit 7 and emulate
		 */
	}

	return ret;
}

static inline  uint32_t x2apic_msr_to_regoff(uint32_t msr)
{

	return (((msr - 0x800U) & 0x3FFU) << 4U);
}

/*
 * If x2apic is pass-thru to guests, we have to special case the following
 * 1. INIT Delivery mode
 * 2. SIPI Delivery mode
 * For all other cases, send IPI on the wire.
 * No shorthand and Physical destination mode are only supported.
 */

static int32_t
vlapic_x2apic_pt_icr_access(struct acrn_vm *vm, uint64_t val)
{
	uint32_t papic_id, vapic_id = (uint32_t)(val >> 32U);
	uint32_t icr_low = (uint32_t)val;
	uint32_t mode = icr_low & APIC_DELMODE_MASK;
	uint16_t vcpu_id;
	struct acrn_vcpu *target_vcpu;
	bool phys;
	uint32_t shorthand;
	int32_t ret = -1;

	phys = ((icr_low & APIC_DESTMODE_LOG) == 0UL);
	shorthand = icr_low & APIC_DEST_MASK;

	if ((phys == false) || (shorthand  != APIC_DEST_DESTFLD)) {
		pr_err("Logical destination mode or shorthands \
				not supported in ICR forpartition mode\n");
		/*
		 * TODO: To support logical destination and shorthand modes
		 */
	} else {
		vcpu_id = vm_apicid2vcpu_id(vm, vapic_id);
		if ((vcpu_id < vm->hw.created_vcpus) && (vm->hw.vcpu_array[vcpu_id].state != VCPU_OFFLINE)) {
			target_vcpu = vcpu_from_vid(vm, vcpu_id);

			switch (mode) {
			case APIC_DELMODE_INIT:
				vlapic_process_init_sipi(target_vcpu, mode, icr_low);
			break;
			case APIC_DELMODE_STARTUP:
				vlapic_process_init_sipi(target_vcpu, mode, icr_low);
			break;
			default:
				papic_id = per_cpu(lapic_id, target_vcpu->pcpu_id);
				dev_dbg(ACRN_DBG_LAPICPT,
					"%s vapic_id: 0x%08lx papic_id: 0x%08lx icr_low:0x%08lx",
					 __func__, vapic_id, papic_id, icr_low);
				msr_write(MSR_IA32_EXT_APIC_ICR, (((uint64_t)papic_id) << 32U) | icr_low);
			break;
			}
			ret = 0;
		}
	}
	return ret;
}

int32_t vlapic_x2apic_read(struct acrn_vcpu *vcpu, uint32_t msr, uint64_t *val)
{
	struct acrn_vlapic *vlapic;
	uint32_t offset;
	int32_t error = -1;

	/*
	 * If vLAPIC is in xAPIC mode and guest tries to access x2APIC MSRs
	 * inject a GP to guest
	 */
	vlapic = vcpu_vlapic(vcpu);
	switch (msr) {
	case MSR_IA32_EXT_APIC_LDR:
	case MSR_IA32_EXT_XAPICID:
		offset = x2apic_msr_to_regoff(msr);
		error = vlapic_read(vlapic, offset, val);
		break;
	default:
		pr_err("%s: unexpected MSR[0x%x] read with lapic_pt", __func__, msr);
		break;
	}

	return error;
}

int32_t vlapic_x2apic_write(struct acrn_vcpu *vcpu, uint32_t msr, uint64_t val)
{
	struct acrn_vlapic *vlapic;
	uint32_t offset;
	int32_t error = -1;

	/*
	 * If vLAPIC is in xAPIC mode and guest tries to access x2APIC MSRs
	 * inject a GP to guest
	 */
	vlapic = vcpu_vlapic(vcpu);
	switch (msr) {
	case MSR_IA32_EXT_APIC_ICR:
		error = vlapic_x2apic_pt_icr_access(vcpu->vm, val);
		break;
	default:
		pr_err("%s: unexpected MSR[0x%x] write with lapic_pt", __func__, msr);
		break;
	}

	return error;
}

int32_t vlapic_create(struct acrn_vcpu *vcpu)
{
	vcpu->arch.vlapic.vm = vcpu->vm;
	vcpu->arch.vlapic.vcpu = vcpu;

	vlapic_init(vcpu_vlapic(vcpu));
	return 0;
}
