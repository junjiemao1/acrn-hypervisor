/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <types.h>
#include <errno.h>
#include <bits.h>
#include <vm.h>
#include <vtd.h>
#include <per_cpu.h>
#include <ioapic.h>

/*
 * lookup a ptdev entry by sid
 * Before adding a ptdev remapping, should lookup by physical sid to check
 * whether the resource has been token by others.
 * When updating a ptdev remapping, should lookup by virtual sid to check
 * whether this resource is valid.
 * @pre: vm must be NULL when lookup by physical sid, otherwise,
 * vm must not be NULL when lookup by virtual sid.
 */
static inline struct ptirq_remapping_info *
ptirq_lookup_entry_by_sid(uint32_t intr_type,
		const union source_id *sid, const struct acrn_vm *vm)
{
	uint16_t idx;
	struct ptirq_remapping_info *entry;
	struct ptirq_remapping_info *entry_found = NULL;

	for (idx = 0U; idx < CONFIG_MAX_PT_IRQ_ENTRIES; idx++) {
		entry = &ptirq_entries[idx];
		if (!is_entry_active(entry)) {
			continue;
		}
		if ((intr_type == entry->intr_type) &&
			((vm == NULL) ?
			(sid->value == entry->phys_sid.value) :
			((vm == entry->vm) &&
			(sid->value == entry->virt_sid.value)))) {
			entry_found = entry;
			break;
		}
	}
	return entry_found;
}

static uint32_t calculate_logical_dest_mask(uint64_t pdmask)
{
	uint32_t dest_mask = 0UL;
	uint64_t pcpu_mask = pdmask;
	uint16_t pcpu_id;

	pcpu_id = ffs64(pcpu_mask);
	while (pcpu_id != INVALID_BIT_INDEX) {
		bitmap_clear_nolock(pcpu_id, &pcpu_mask);
		dest_mask |= per_cpu(lapic_ldr, pcpu_id);
		pcpu_id = ffs64(pcpu_mask);
	}
	return dest_mask;
}

/**
 * @pre entry != NULL
 */
static void ptirq_free_irte(const struct ptirq_remapping_info *entry)
{
	struct intr_source intr_src;

	intr_src.is_msi = true;
	intr_src.src.msi.value = entry->phys_sid.msi_id.bdf;

	dmar_free_irte(intr_src, (uint16_t)entry->allocated_pirq);
}

static void ptirq_build_physical_msi(struct acrn_vm *vm, struct ptirq_msi_info *info,
		const struct ptirq_remapping_info *entry, uint32_t vector)
{
	uint64_t vdmask, pdmask;
	uint32_t dest, delmode, dest_mask;
	bool phys;
	union dmar_ir_entry irte;
	union irte_index ir_index;
	int32_t ret;
	struct intr_source intr_src;

	/* get physical destination cpu mask */
	dest = info->vmsi_addr.bits.dest_field;
	phys = (info->vmsi_addr.bits.dest_mode == MSI_ADDR_DESTMODE_PHYS);

	vlapic_calc_dest(vm, &vdmask, false, dest, phys, false);
	pdmask = vcpumask2pcpumask(vm, vdmask);

	/* get physical delivery mode */
	delmode = info->vmsi_data.bits.delivery_mode;
	if ((delmode != MSI_DATA_DELMODE_FIXED) && (delmode != MSI_DATA_DELMODE_LOPRI)) {
		delmode = MSI_DATA_DELMODE_LOPRI;
	}

	dest_mask = calculate_logical_dest_mask(pdmask);

	/* Using phys_irq as index in the corresponding IOMMU */
	irte.entry.lo_64 = 0UL;
	irte.entry.hi_64 = 0UL;
	irte.bits.vector = vector;
	irte.bits.delivery_mode = delmode;
	irte.bits.dest_mode = MSI_ADDR_DESTMODE_LOGICAL;
	irte.bits.rh = MSI_ADDR_RH;
	irte.bits.dest = dest_mask;

	intr_src.is_msi = true;
	intr_src.src.msi.value = entry->phys_sid.msi_id.bdf;
	ret = dmar_assign_irte(intr_src, irte, (uint16_t)entry->allocated_pirq);

	if (ret == 0) {
		/*
		 * Update the MSI interrupt source to point to the IRTE
		 * SHV is set to 0 as ACRN disables MMC (Multi-Message Capable
		 * for MSI devices.
		 */
		info->pmsi_data.full = 0U;
		ir_index.index = (uint16_t)entry->allocated_pirq;

		info->pmsi_addr.full = 0UL;
		info->pmsi_addr.ir_bits.intr_index_high = ir_index.bits.index_high;
		info->pmsi_addr.ir_bits.shv = 0U;
		info->pmsi_addr.ir_bits.intr_format = 0x1U;
		info->pmsi_addr.ir_bits.intr_index_low = ir_index.bits.index_low;
		info->pmsi_addr.ir_bits.constant = 0xFEEU;
	} else {
		/* In case there is no corresponding IOMMU, for example, if the
		 * IOMMU is ignored, pass the MSI info in Compatibility Format
		 */
		info->pmsi_data = info->vmsi_data;
		info->pmsi_data.bits.delivery_mode = delmode;
		info->pmsi_data.bits.vector = vector;

		info->pmsi_addr = info->vmsi_addr;
		info->pmsi_addr.bits.dest_field = dest_mask;
		info->pmsi_addr.bits.rh = MSI_ADDR_RH;
		info->pmsi_addr.bits.dest_mode = MSI_ADDR_DESTMODE_LOGICAL;
	}
	dev_dbg(ACRN_DBG_IRQ, "MSI %s addr:data = 0x%llx:%x(V) -> 0x%llx:%x(P)",
		(info->pmsi_addr.ir_bits.intr_format != 0U) ? " Remappable Format" : "Compatibility Format",
		info->vmsi_addr.full, info->vmsi_data.full,
		info->pmsi_addr.full, info->pmsi_data.full);
}

/* add msix entry for a vm, based on msi id (phys_bdf+msix_index)
 * - if the entry not be added by any vm, allocate it
 * - if the entry already be added by sos_vm, then change the owner to current vm
 * - if the entry already be added by other vm, return NULL
 */
static struct ptirq_remapping_info *add_msix_remapping(struct acrn_vm *vm,
	uint16_t virt_bdf, uint16_t phys_bdf, uint32_t entry_nr)
{
	struct ptirq_remapping_info *entry;
	DEFINE_MSI_SID(phys_sid, phys_bdf, entry_nr);
	DEFINE_MSI_SID(virt_sid, virt_bdf, entry_nr);

	entry = ptirq_lookup_entry_by_sid(PTDEV_INTR_MSI, &phys_sid, NULL);
	if (entry == NULL) {
		if (ptirq_lookup_entry_by_sid(PTDEV_INTR_MSI, &virt_sid, vm) != NULL) {
			pr_err("MSIX re-add vbdf%x", virt_bdf);
		} else {
			entry = ptirq_alloc_entry(vm, PTDEV_INTR_MSI);
			if (entry != NULL) {
				entry->phys_sid.value = phys_sid.value;
				entry->virt_sid.value = virt_sid.value;
				entry->release_cb = ptirq_free_irte;

				/* update msi source and active entry */
				if (ptirq_activate_entry(entry, IRQ_INVALID) < 0) {
					ptirq_release_entry(entry);
					entry = NULL;
				}
			}
		}
	} else if (entry->vm != vm) {
		pr_err("MSIX pbdf%x idx=%d already in vm%d with vbdf%x, not able to add into vm%d with vbdf%x",
			entry->phys_sid.msi_id.bdf, entry->phys_sid.msi_id.entry_nr, entry->vm->vm_id,
			entry->virt_sid.msi_id.bdf, vm->vm_id, virt_bdf);

		pr_err("msix entry pbdf%x idx%d already in vm%d", phys_bdf, entry_nr, entry->vm->vm_id);
		entry = NULL;
	} else {
		/* The mapping has already been added to the VM. No action
		 * required. */
	}

	dev_dbg(ACRN_DBG_IRQ, "VM%d MSIX add vector mapping vbdf%x:pbdf%x idx=%d",
		vm->vm_id, virt_bdf, phys_bdf, entry_nr);

	return entry;
}

/* deactive & remove mapping entry of vbdf:entry_nr for vm */
static void
remove_msix_remapping(const struct acrn_vm *vm, uint16_t virt_bdf, uint32_t entry_nr)
{
	struct ptirq_remapping_info *entry;
	DEFINE_MSI_SID(virt_sid, virt_bdf, entry_nr);
	struct intr_source intr_src;

	entry = ptirq_lookup_entry_by_sid(PTDEV_INTR_MSI, &virt_sid, vm);
	if (entry != NULL) {
		if (is_entry_active(entry)) {
			/*TODO: disable MSIX device when HV can in future */
			ptirq_deactivate_entry(entry);
		}

		intr_src.is_msi = true;
		intr_src.src.msi.value = entry->phys_sid.msi_id.bdf;
		dmar_free_irte(intr_src, (uint16_t)entry->allocated_pirq);

		dev_dbg(ACRN_DBG_IRQ,
			"VM%d MSIX remove vector mapping vbdf-pbdf:0x%x-0x%x idx=%d",
			entry->vm->vm_id, virt_bdf,
			entry->phys_sid.msi_id.bdf, entry_nr);

		ptirq_release_entry(entry);
	}

}

/* Main entry for PCI device assignment with MSI and MSI-X
 * MSI can up to 8 vectors and MSI-X can up to 1024 Vectors
 * We use entry_nr to indicate coming vectors
 * entry_nr = 0 means first vector
 * user must provide bdf and entry_nr
 */
int32_t ptirq_msix_remap(struct acrn_vm *vm, uint16_t virt_bdf, uint16_t phys_bdf,
				uint16_t entry_nr, struct ptirq_msi_info *info)
{
	struct ptirq_remapping_info *entry;
	DEFINE_MSI_SID(virt_sid, virt_bdf, entry_nr);
	int32_t ret = -ENODEV;

	/*
	 * Device Model should pre-hold the mapping entries by calling
	 * ptirq_add_msix_remapping for UOS.
	 *
	 * For SOS(sos_vm), it adds the mapping entries at runtime, if the
	 * entry already be held by others, return error.
	 */
	spinlock_obtain(&ptdev_lock);
	entry = ptirq_lookup_entry_by_sid(PTDEV_INTR_MSI, &virt_sid, vm);
	if (entry == NULL) {
		entry = add_msix_remapping(vm, virt_bdf, phys_bdf, entry_nr);
		if (entry == NULL) {
			pr_err("dev-assign: msi entry exist in others");
		}
	}
	spinlock_release(&ptdev_lock);

	if (entry != NULL) {
		ret = 0;
		if (is_entry_active(entry) && (info->vmsi_data.full == 0U)) {
			/* handle destroy case */
			info->pmsi_data.full = 0U;
		} else {
			enum vm_vlapic_state vlapic_state = check_vm_vlapic_state(vm);
			if (vlapic_state == VM_VLAPIC_X2APIC) {
				/*
				 * All the vCPUs are in x2APIC mode and LAPIC is Pass-through
				 * Use guest vector to program the interrupt source
				 */
				ptirq_build_physical_msi(vm, info, entry, (uint32_t)info->vmsi_data.bits.vector);
			} else if (vlapic_state == VM_VLAPIC_XAPIC) {
				/*
				 * All the vCPUs are in xAPIC mode and LAPIC is emulated
				 * Use host vector to program the interrupt source
				 */
				ptirq_build_physical_msi(vm, info, entry, irq_to_vector(entry->allocated_pirq));
			} else if (vlapic_state == VM_VLAPIC_TRANSITION) {
				/*
				 * vCPUs are in middle of transition, so do not program interrupt source
				 * TODO: Devices programmed during transistion do not work after transition
				 * as device is not programmed with interrupt info. Need to implement a
				 * method to get interrupts working after transition.
				 */
				ret = -EFAULT;
			} else {
				/* Do nothing for VM_VLAPIC_DISABLED */
				ret = -EFAULT;
			}

			if (ret == 0) {
				entry->msi = *info;
				dev_dbg(ACRN_DBG_IRQ, "PCI %x:%x.%x MSI VR[%d] 0x%x->0x%x assigned to vm%d",
					pci_bus(virt_bdf), pci_slot(virt_bdf), pci_func(virt_bdf), entry_nr,
					info->vmsi_data.bits.vector, irq_to_vector(entry->allocated_pirq), entry->vm->vm_id);
			}
		}
	}

	return ret;
}

/*
 * @pre vm != NULL
 */
void ptirq_remove_msix_remapping(const struct acrn_vm *vm, uint16_t virt_bdf,
		uint32_t vector_count)
{
	uint32_t i;

	for (i = 0U; i < vector_count; i++) {
		spinlock_obtain(&ptdev_lock);
		remove_msix_remapping(vm, virt_bdf, i);
		spinlock_release(&ptdev_lock);
	}
}
