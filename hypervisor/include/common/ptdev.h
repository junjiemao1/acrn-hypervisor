/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef PTDEV_H
#define PTDEV_H
#include <list.h>
#include <spinlock.h>
#include <pci.h>
#include <timer.h>

#define ACTIVE_FLAG 0x1U /* any non zero should be okay */

#define PTDEV_INTR_MSI		(1U << 0U)

#define INVALID_PTDEV_ENTRY_ID 0xffffU

#define DEFINE_MSI_SID(name, a, b)	\
union source_id (name) = {.msi_id = {.bdf = (a), .entry_nr = (b)} }

union source {
	uint16_t ioapic_id;
	union pci_bdf msi;
};

struct intr_source {
	bool is_msi;
	union source src;
};

union irte_index {
	uint16_t index;
	struct {
		uint16_t index_low:15;
		uint16_t index_high:1;
	} bits __packed;
};

union source_id {
	uint64_t value;
	struct {
		uint16_t bdf;
		uint16_t entry_nr;
		uint32_t reserved;
	} msi_id;
	struct {
		uint32_t pin;
		uint32_t src;
	} intx_id;
};

/*
 * Macros for bits in union msi_addr_reg
 */

#define	MSI_ADDR_RH			0x1U	/* Redirection Hint */
#define	MSI_ADDR_DESTMODE_LOGICAL	0x1U	/* Destination Mode: Logical*/
#define	MSI_ADDR_DESTMODE_PHYS		0x0U	/* Destination Mode: Physical*/

union msi_addr_reg {
	uint64_t full;
	struct {
		uint32_t rsvd_1:2;
		uint32_t dest_mode:1;
		uint32_t rh:1;
		uint32_t rsvd_2:8;
		uint32_t dest_field:8;
		uint32_t addr_base:12;
		uint32_t hi_32;
	} bits __packed;
	struct {
		uint32_t rsvd_1:2;
		uint32_t intr_index_high:1;
		uint32_t shv:1;
		uint32_t intr_format:1;
		uint32_t intr_index_low:15;
		uint32_t constant:12;
		uint32_t hi_32;
	} ir_bits __packed;

};

/*
 * Macros for bits in union msi_data_reg
 */

#define MSI_DATA_DELMODE_FIXED		0x0U	/* Delivery Mode: Fixed */
#define MSI_DATA_DELMODE_LOPRI		0x1U	/* Delivery Mode: Low Priority */

union msi_data_reg {
	uint32_t full;
	struct {
		uint32_t vector:8;
		uint32_t delivery_mode:3;
		uint32_t rsvd_1:3;
		uint32_t level:1;
		uint32_t trigger_mode:1;
		uint32_t rsvd_2:16;
	} bits __packed;
};

/* entry per guest virt vector */
struct ptirq_msi_info {
	union msi_addr_reg vmsi_addr; /* virt msi_addr */
	union msi_data_reg vmsi_data; /* virt msi_data */
	union msi_addr_reg pmsi_addr; /* phys msi_addr */
	union msi_data_reg pmsi_data; /* phys msi_data */
};

struct ptirq_remapping_info;
typedef void (*ptirq_arch_release_fn_t)(const struct ptirq_remapping_info *entry);

/* entry per each allocated irq/vector
 * it represents a pass-thru device's remapping data entry which collecting
 * information related with its vm and msi/intx mapping & interaction nodes
 * with interrupt handler and softirq.
 */
struct ptirq_remapping_info {
	uint16_t ptdev_entry_id;
	uint32_t intr_type;
	union source_id phys_sid;
	union source_id virt_sid;
	struct acrn_vm *vm;
	uint32_t active;	/* 1=active, 0=inactive and to free*/
	uint32_t allocated_pirq;
	struct list_head softirq_node;
	struct ptirq_msi_info msi;

	uint64_t intr_count;
	ptirq_arch_release_fn_t release_cb;
};

extern struct ptirq_remapping_info ptirq_entries[CONFIG_MAX_PT_IRQ_ENTRIES];
extern spinlock_t ptdev_lock;

bool is_entry_active(const struct ptirq_remapping_info *entry);
void ptdev_release_all_entries(const struct acrn_vm *vm);

struct ptirq_remapping_info *ptirq_alloc_entry(struct acrn_vm *vm, uint32_t intr_type);
void ptirq_release_entry(struct ptirq_remapping_info *entry);
int32_t ptirq_activate_entry(struct ptirq_remapping_info *entry, uint32_t phys_irq);
void ptirq_deactivate_entry(struct ptirq_remapping_info *entry);

#endif /* PTDEV_H */
