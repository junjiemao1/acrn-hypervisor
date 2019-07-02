/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <vm.h>
#include <softirq.h>
#include <ptdev.h>
#include <irq.h>
#include <atomic.h>
#include <logmsg.h>

#define PTIRQ_BITMAP_ARRAY_SIZE	INT_DIV_ROUNDUP(CONFIG_MAX_PT_IRQ_ENTRIES, 64U)
struct ptirq_remapping_info ptirq_entries[CONFIG_MAX_PT_IRQ_ENTRIES];
static uint64_t ptirq_entry_bitmaps[PTIRQ_BITMAP_ARRAY_SIZE];
spinlock_t ptdev_lock;

bool is_entry_active(const struct ptirq_remapping_info *entry)
{
	return atomic_load32(&entry->active) == ACTIVE_FLAG;
}

static inline uint16_t ptirq_alloc_entry_id(void)
{
	uint16_t id = (uint16_t)ffz64_ex(ptirq_entry_bitmaps, CONFIG_MAX_PT_IRQ_ENTRIES);

	while (id < CONFIG_MAX_PT_IRQ_ENTRIES) {
		if (!bitmap_test_and_set_lock((id & 0x3FU), &ptirq_entry_bitmaps[id >> 6U])) {
			break;
		}
		id = (uint16_t)ffz64_ex(ptirq_entry_bitmaps, CONFIG_MAX_PT_IRQ_ENTRIES);
	}

	return (id < CONFIG_MAX_PT_IRQ_ENTRIES) ? id: INVALID_PTDEV_ENTRY_ID;
}

struct ptirq_remapping_info *ptirq_alloc_entry(struct acrn_vm *vm, uint32_t intr_type)
{
	struct ptirq_remapping_info *entry = NULL;
	uint16_t ptirq_id = ptirq_alloc_entry_id();

	if (ptirq_id < CONFIG_MAX_PT_IRQ_ENTRIES) {
		entry = &ptirq_entries[ptirq_id];
		(void)memset((void *)entry, 0U, sizeof(struct ptirq_remapping_info));
		entry->ptdev_entry_id = ptirq_id;
		entry->intr_type = intr_type;
		entry->vm = vm;
		entry->intr_count = 0UL;

		INIT_LIST_HEAD(&entry->softirq_node);

		atomic_clear32(&entry->active, ACTIVE_FLAG);
	} else {
		pr_err("Alloc ptdev irq entry failed");
	}

	return entry;
}

void ptirq_release_entry(struct ptirq_remapping_info *entry)
{
	uint64_t rflags;

	spinlock_irqsave_obtain(&entry->vm->softirq_dev_lock, &rflags);
	list_del_init(&entry->softirq_node);
	spinlock_irqrestore_release(&entry->vm->softirq_dev_lock, rflags);

	bitmap_clear_nolock((entry->ptdev_entry_id) & 0x3FU,
		&ptirq_entry_bitmaps[((entry->ptdev_entry_id) & 0x3FU) >> 6U]);

	(void)memset((void *)entry, 0U, sizeof(struct ptirq_remapping_info));
}

/* active intr with irq registering */
int32_t ptirq_activate_entry(struct ptirq_remapping_info *entry, uint32_t phys_irq)
{
	int32_t retval;

	/* register and allocate host vector/irq */
	retval = request_irq(phys_irq, NULL, (void *)entry, IRQF_PT);

	if (retval < 0) {
		pr_err("request irq failed, please check!, phys-irq=%d", phys_irq);
	} else {
		entry->allocated_pirq = (uint32_t)retval;
		atomic_set32(&entry->active, ACTIVE_FLAG);
	}

	return retval;
}

void ptirq_deactivate_entry(struct ptirq_remapping_info *entry)
{
	atomic_clear32(&entry->active, ACTIVE_FLAG);
	free_irq(entry->allocated_pirq);
}

void ptdev_release_all_entries(const struct acrn_vm *vm)
{
	struct ptirq_remapping_info *entry;
	uint16_t idx;

	/* VM already down */
	for (idx = 0U; idx < CONFIG_MAX_PT_IRQ_ENTRIES; idx++) {
		entry = &ptirq_entries[idx];
		if ((entry->vm == vm) && is_entry_active(entry)) {
			spinlock_obtain(&ptdev_lock);
			if (entry->release_cb != NULL) {
				entry->release_cb(entry);
			}
			ptirq_deactivate_entry(entry);
			ptirq_release_entry(entry);
			spinlock_release(&ptdev_lock);
		}
	}

}
