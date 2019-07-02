/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <types.h>
#include <errno.h>
#include <spinlock.h>
#include <ioapic.h>
#include <irq.h>
#include <pgtable.h>
#include <io.h>
#include <mmu.h>
#include <acpi.h>
#include <logmsg.h>

#define NR_MAX_GSI		(CONFIG_MAX_IOAPIC_NUM * CONFIG_MAX_IOAPIC_LINES)

static struct gsi_table gsi_table_data[NR_MAX_GSI];
static uint32_t ioapic_nr_gsi;
static spinlock_t ioapic_lock;

/*
 * the irq to ioapic pin mapping should extract from ACPI MADT table
 * hardcoded here
 */
static const uint32_t legacy_irq_to_pin[NR_LEGACY_IRQ] = {
	2U, /* IRQ0*/
	1U, /* IRQ1*/
	0U, /* IRQ2 connected to Pin0 (ExtInt source of PIC) if existing */
	3U, /* IRQ3*/
	4U, /* IRQ4*/
	5U, /* IRQ5*/
	6U, /* IRQ6*/
	7U, /* IRQ7*/
	8U, /* IRQ8*/
	9U, /* IRQ9*/
	10U, /* IRQ10*/
	11U, /* IRQ11*/
	12U, /* IRQ12*/
	13U, /* IRQ13*/
	14U, /* IRQ14*/
	15U, /* IRQ15*/
};

static const uint32_t legacy_irq_trigger_mode[NR_LEGACY_IRQ] = {
	IOAPIC_RTE_TRGRMODE_EDGE, /* IRQ0*/
	IOAPIC_RTE_TRGRMODE_EDGE, /* IRQ1*/
	IOAPIC_RTE_TRGRMODE_EDGE, /* IRQ2*/
	IOAPIC_RTE_TRGRMODE_EDGE, /* IRQ3*/
	IOAPIC_RTE_TRGRMODE_EDGE, /* IRQ4*/
	IOAPIC_RTE_TRGRMODE_EDGE, /* IRQ5*/
	IOAPIC_RTE_TRGRMODE_EDGE, /* IRQ6*/
	IOAPIC_RTE_TRGRMODE_EDGE, /* IRQ7*/
	IOAPIC_RTE_TRGRMODE_EDGE, /* IRQ8*/
	IOAPIC_RTE_TRGRMODE_LEVEL, /* IRQ9*/
	IOAPIC_RTE_TRGRMODE_EDGE, /* IRQ10*/
	IOAPIC_RTE_TRGRMODE_EDGE, /* IRQ11*/
	IOAPIC_RTE_TRGRMODE_EDGE, /* IRQ12*/
	IOAPIC_RTE_TRGRMODE_EDGE, /* IRQ13*/
	IOAPIC_RTE_TRGRMODE_EDGE, /* IRQ14*/
	IOAPIC_RTE_TRGRMODE_EDGE, /* IRQ15*/
};

static struct ioapic_info ioapic_array[CONFIG_MAX_IOAPIC_NUM];
static uint16_t ioapic_num;

static void *map_ioapic(uint64_t ioapic_paddr)
{
	/* At some point we may need to translate this paddr to a vaddr.
	 * 1:1 mapping for now.
	 */
	return hpa2hva(ioapic_paddr);
}

static inline uint32_t
ioapic_read_reg32(void *ioapic_base, const uint32_t offset)
{
	uint32_t v;
	uint64_t rflags;

	spinlock_irqsave_obtain(&ioapic_lock, &rflags);

	/* Write IOREGSEL */
	mmio_write32(offset, ioapic_base + IOAPIC_REGSEL);
	/* Read  IOWIN */
	v = mmio_read32(ioapic_base + IOAPIC_WINDOW);

	spinlock_irqrestore_release(&ioapic_lock, rflags);
	return v;
}

static inline void
ioapic_write_reg32(void *ioapic_base, const uint32_t offset, const uint32_t value)
{
	uint64_t rflags;

	spinlock_irqsave_obtain(&ioapic_lock, &rflags);

	/* Write IOREGSEL */
	mmio_write32(offset, ioapic_base + IOAPIC_REGSEL);
	/* Write IOWIN */
	mmio_write32(value, ioapic_base + IOAPIC_WINDOW);

	spinlock_irqrestore_release(&ioapic_lock, rflags);
}

void ioapic_get_rte_entry(void *ioapic_addr, uint32_t pin, union ioapic_rte *rte)
{
	uint32_t rte_addr = (pin * 2U) + 0x10U;
	rte->u.lo_32 = ioapic_read_reg32(ioapic_addr, rte_addr);
	rte->u.hi_32 = ioapic_read_reg32(ioapic_addr, rte_addr + 1U);
}

static inline void
ioapic_set_rte_entry(void *ioapic_addr,
		uint32_t pin, union ioapic_rte rte)
{
	uint32_t rte_addr = (pin * 2U) + 0x10U;
	ioapic_write_reg32(ioapic_addr, rte_addr, rte.u.lo_32);
	ioapic_write_reg32(ioapic_addr, rte_addr + 1U, rte.u.hi_32);
}

static inline union ioapic_rte
create_rte_for_legacy_irq(uint32_t irq, uint32_t vr)
{
	union ioapic_rte rte;

	/* Legacy IRQ 0-15 setup, default masked
	 * are actually defined in either MPTable or ACPI MADT table
	 * before we have ACPI table parsing in HV we use common hardcode
	 */

	rte.full = 0UL;
	rte.bits.intr_mask  = IOAPIC_RTE_MASK_SET;
	rte.bits.trigger_mode = legacy_irq_trigger_mode[irq];
	rte.bits.dest_mode = DEFAULT_DEST_MODE;
	rte.bits.delivery_mode = DEFAULT_DELIVERY_MODE;
	rte.bits.vector = vr;

	/* Fixed to active high */
	rte.bits.intr_polarity = IOAPIC_RTE_INTPOL_AHI;

	/* Dest field: legacy irq fixed to CPU0 */
	rte.bits.dest_field = 1U;

	return rte;
}

static inline union ioapic_rte
create_rte_for_gsi_irq(uint32_t irq, uint32_t vr)
{
	union ioapic_rte rte;

	rte.full = 0UL;

	if (irq < NR_LEGACY_IRQ) {
		rte = create_rte_for_legacy_irq(irq, vr);
	} else {
		/* irq default masked, level trig */
		rte.bits.intr_mask  = IOAPIC_RTE_MASK_SET;
		rte.bits.trigger_mode = IOAPIC_RTE_TRGRMODE_LEVEL;
		rte.bits.dest_mode = DEFAULT_DEST_MODE;
		rte.bits.delivery_mode = DEFAULT_DELIVERY_MODE;
		rte.bits.vector = vr;

		/* Fixed to active high */
		rte.bits.intr_polarity = IOAPIC_RTE_INTPOL_AHI;

		/* Dest field */
		rte.bits.dest_field = ALL_CPUS_MASK;
	}

	return rte;
}

static void ioapic_set_routing(uint32_t gsi, uint32_t vr)
{
	void *addr;
	union ioapic_rte rte;

	addr = gsi_table_data[gsi].addr;
	rte = create_rte_for_gsi_irq(gsi, vr);
	ioapic_set_rte_entry(addr, gsi_table_data[gsi].pin, rte);

	if (rte.bits.trigger_mode == IOAPIC_RTE_TRGRMODE_LEVEL) {
		set_irq_trigger_mode(gsi, true);
	} else {
		set_irq_trigger_mode(gsi, false);
	}

	dev_dbg(ACRN_DBG_IRQ, "GSI: irq:%d pin:%hhu rte:%lx",
		gsi, gsi_table_data[gsi].pin,
		rte.full);
}

bool ioapic_irq_is_gsi(uint32_t irq)
{
	return irq < ioapic_nr_gsi;
}

static void
ioapic_irq_gsi_mask_unmask(uint32_t irq, bool mask)
{
	void *addr = NULL;
	uint32_t pin;
	union ioapic_rte rte;

	if (ioapic_irq_is_gsi(irq)) {
		addr = gsi_table_data[irq].addr;
		pin = gsi_table_data[irq].pin;

		if (addr != NULL) {
			ioapic_get_rte_entry(addr, pin, &rte);
			if (mask) {
				rte.bits.intr_mask = IOAPIC_RTE_MASK_SET;
			} else {
				rte.bits.intr_mask = IOAPIC_RTE_MASK_CLR;
			}
			ioapic_set_rte_entry(addr, pin, rte);
			dev_dbg(ACRN_DBG_PTIRQ, "update: irq:%d pin:%hhu rte:%lx",
				irq, pin, rte.full);
		} else {
			dev_dbg(ACRN_DBG_PTIRQ, "NULL Address returned from gsi_table_data");
		}
	}
}

void ioapic_gsi_mask_irq(uint32_t irq)
{
	ioapic_irq_gsi_mask_unmask(irq, true);
}

void ioapic_gsi_unmask_irq(uint32_t irq)
{
	ioapic_irq_gsi_mask_unmask(irq, false);
}

static uint32_t
ioapic_nr_pins(void *ioapic_base)
{
	uint32_t version;
	uint32_t nr_pins;

	version = ioapic_read_reg32(ioapic_base, IOAPIC_VER);
	dev_dbg(ACRN_DBG_IRQ, "IOAPIC version: %x", version);

	/* The 23:16 bits in the version register is the highest entry in the
	 * I/O redirection table, which is 1 smaller than the number of
	 * interrupt input pins. */
	nr_pins = (((version & IOAPIC_MAX_RTE_MASK) >> MAX_RTE_SHIFT) + 1U);

	return nr_pins;
}

int32_t init_ioapic_id_info(void)
{
	int32_t ret = 0;
	uint8_t ioapic_id;
	void *addr;
	uint32_t nr_pins, gsi;

	ioapic_num = parse_madt_ioapic(&ioapic_array[0]);
	if (ioapic_num <= (uint16_t)CONFIG_MAX_IOAPIC_NUM) {
		/*
		 * Iterate thru all the IO-APICs on the platform
		 * Check the number of pins available on each IOAPIC is less
		 * than the CONFIG_MAX_IOAPIC_LINES
		 */

		gsi = 0U;
		for (ioapic_id = 0U; ioapic_id < ioapic_num; ioapic_id++) {
			addr = map_ioapic(ioapic_array[ioapic_id].addr);
			hv_access_memory_region_update((uint64_t)addr, PAGE_SIZE);

			nr_pins = ioapic_nr_pins(addr);
			if (nr_pins <= (uint32_t) CONFIG_MAX_IOAPIC_LINES) {
				gsi += nr_pins;
				ioapic_array[ioapic_id].nr_pins = nr_pins;
			} else {
				pr_err ("Pin count %x of IOAPIC with %x > CONFIG_MAX_IOAPIC_LINES, bump up CONFIG_MAX_IOAPIC_LINES!",
							nr_pins, ioapic_array[ioapic_id].id);
				ret = -EINVAL;
				break;
			}
		}

		/*
		 * Check if total pin count, can be inferred by GSI, is
		 * atleast same as the number of Legacy IRQs, NR_LEGACY_IRQ
		 */

		if (ret == 0) {
			if (gsi < (uint32_t) NR_LEGACY_IRQ) {
				pr_err ("Total pin count (%x) is less than NR_LEGACY_IRQ!", gsi);
				ret = -EINVAL;
			}
		}
	} else {
		pr_err ("Number of IOAPIC on platform %x > CONFIG_MAX_IOAPIC_NUM, try bumping up CONFIG_MAX_IOAPIC_NUM!",
						ioapic_num);
		ret = -EINVAL;
	}

	return ret;
}

void ioapic_setup_irqs(void)
{
	uint8_t ioapic_id;
	uint32_t gsi = 0U;
	uint32_t vr;

	spinlock_init(&ioapic_lock);

	for (ioapic_id = 0U;
	     ioapic_id < ioapic_num; ioapic_id++) {
		void *addr;
		uint32_t pin, nr_pins;

		addr = map_ioapic(ioapic_array[ioapic_id].addr);

		nr_pins = ioapic_array[ioapic_id].nr_pins;
		for (pin = 0U; pin < nr_pins; pin++) {
			gsi_table_data[gsi].ioapic_id = ioapic_array[ioapic_id].id;
			gsi_table_data[gsi].addr = addr;

			if (gsi < NR_LEGACY_IRQ) {
				gsi_table_data[gsi].pin =
					legacy_irq_to_pin[gsi] & 0xffU;
			} else {
				gsi_table_data[gsi].pin = pin;
			}

			/* pinned irq before use it */
			if (alloc_irq_num(gsi) == IRQ_INVALID) {
				pr_err("failed to alloc IRQ[%d]", gsi);
				gsi++;
				continue;
			}

			/* assign vector for this GSI
			 * for legacy irq, reserved vector and never free
			 */
			if (gsi < NR_LEGACY_IRQ) {
				vr = alloc_irq_vector(gsi);
				if (vr == VECTOR_INVALID) {
					pr_err("failed to alloc VR");
					gsi++;
					continue;
				}
			} else {
				vr = 0U; /* not to allocate VR right now */
			}

			ioapic_set_routing(gsi, vr);
			gsi++;
		}
	}

	/* system max gsi numbers */
	ioapic_nr_gsi = gsi;
}
