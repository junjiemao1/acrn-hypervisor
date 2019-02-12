/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <types.h>
#include <acrn_hv_defs.h>
#include <page.h>
#include <e820.h>
#include <mmu.h>
#include <multiboot.h>
#include <logmsg.h>

/*
 * e820.c contains the related e820 operations; like HV to get memory info for its MMU setup;
 * and hide HV memory from SOS_VM...
 */

static uint32_t e820_entries_count;
static struct e820_entry e820[E820_MAX_ENTRIES];
static struct e820_mem_params e820_mem;

#define ACRN_DBG_E820	6U

static void obtain_e820_mem_info(void)
{
	uint32_t i;
	struct e820_entry *entry;

	e820_mem.mem_bottom = UINT64_MAX;
	e820_mem.mem_top = 0x0UL;
	e820_mem.total_mem_size = 0UL;

	for (i = 0U; i < e820_entries_count; i++) {
		entry = &e820[i];
		if (e820_mem.mem_bottom > entry->baseaddr) {
			e820_mem.mem_bottom = entry->baseaddr;
		}

		if ((entry->baseaddr + entry->length) > e820_mem.mem_top) {
			e820_mem.mem_top = entry->baseaddr + entry->length;
		}

		if (entry->type == E820_TYPE_RAM) {
			e820_mem.total_mem_size += entry->length;
		}
	}
}

/* get some RAM below 1MB in e820 entries, hide it from sos_vm, return its start address */
uint64_t e820_alloc_low_memory(uint32_t size_arg)
{
	uint32_t i;
	uint32_t size = size_arg;
	uint64_t ret = ACRN_INVALID_HPA;
	struct e820_entry *entry, *new_entry;

	/* We want memory in page boundary and integral multiple of pages */
	size = (((size + PAGE_SIZE) - 1U) >> PAGE_SHIFT) << PAGE_SHIFT;

	for (i = 0U; i < e820_entries_count; i++) {
		entry = &e820[i];
		uint64_t start, end, length;

		start = round_page_up(entry->baseaddr);
		end = round_page_down(entry->baseaddr + entry->length);
		length = end - start;
		length = (end > start) ? (end - start) : 0;

		/* Search for available low memory */
		if ((entry->type != E820_TYPE_RAM) || (length < size) || ((start + size) > MEM_1M)) {
			continue;
		}

		/* found exact size of e820 entry */
		if (length == size) {
			entry->type = E820_TYPE_RESERVED;
			e820_mem.total_mem_size -= size;
			ret = start;
			break;
		}

		/*
		 * found entry with available memory larger than requested
		 * allocate memory from the end of this entry at page boundary
		 */
		new_entry = &e820[e820_entries_count];
		new_entry->type = E820_TYPE_RESERVED;
		new_entry->baseaddr = end - size;
		new_entry->length = (entry->baseaddr + entry->length) - new_entry->baseaddr;

		/* Shrink the existing entry and total available memory */
		entry->length -= new_entry->length;
		e820_mem.total_mem_size -= new_entry->length;
		e820_entries_count++;

	        ret = new_entry->baseaddr;
		break;
	}

	if (ret == ACRN_INVALID_HPA) {
		pr_fatal("Can't allocate memory under 1M from E820\n");
	}
	return ret;
}

struct e820_entry mbi_reserved[3] = { 0 };
uint32_t mbi_reserved_sorted_idx[3] = {0, 1, 2};

/* HV read multiboot header to get e820 entries info and calc total RAM info */
void init_e820(void)
{
	uint32_t i, j, e820_idx;

	if (boot_regs[0] == MULTIBOOT_INFO_MAGIC) {
		/*
		 * Before installing new PML4 table in enable_paging(), HPA->HVA is always 1:1 mapping
		 * and hpa2hva() can't be used to do the conversion. Here we simply treat boot_reg[1] as HPA.
		 */
		uint64_t hpa = (uint64_t)boot_regs[1];
		struct multiboot_info *mbi = (struct multiboot_info *)hpa;

		pr_info("Multiboot info detected\n");
		if ((mbi->mi_flags & MULTIBOOT_INFO_HAS_MMAP) != 0U) {
			/* HPA->HVA is always 1:1 mapping at this moment */
			hpa = (uint64_t)mbi->mi_mmap_addr;
			struct multiboot_mmap *mmap = (struct multiboot_mmap *)hpa;

			e820_entries_count = mbi->mi_mmap_length / sizeof(struct multiboot_mmap);
			if (e820_entries_count > E820_MAX_ENTRIES) {
				pr_err("Too many E820 entries %d\n", e820_entries_count);
				e820_entries_count = E820_MAX_ENTRIES;
			}

			dev_dbg(ACRN_DBG_E820, "mmap length 0x%x addr 0x%x entries %d\n",
				mbi->mi_mmap_length, mbi->mi_mmap_addr, e820_entries_count);

			/* Consider multiboot boot info, mods array and mods[0] as reserved */
			mbi_reserved[0].baseaddr = round_page_down((uint64_t)boot_regs[1]);
			mbi_reserved[0].length = round_page_up(sizeof(struct multiboot_info));

			if ((mbi->mi_flags & MULTIBOOT_INFO_HAS_MODS) != 0U) {
				struct multiboot_module *mods = (struct multiboot_module *)(uint64_t)mbi->mi_mods_addr;

				mbi_reserved[1].baseaddr = round_page_down((uint64_t)mbi->mi_mods_addr);
				mbi_reserved[1].length = round_page_up(mbi->mi_mods_count * sizeof(struct multiboot_module));

				if (mbi->mi_mods_count > 0) {
					mbi_reserved[2].baseaddr = round_page_down(mods[0].mm_mod_start);
					mbi_reserved[2].length = round_page_up(mods[0].mm_mod_end - mods[0].mm_mod_start);
				}
			}

			/* Sort mbi_reserved according to baseaddr */
			for (i = 2U; i > 0U; i--) {
				for (j = 0U; j < i; j++) {
					uint32_t idx_j = mbi_reserved_sorted_idx[j];
					uint32_t idx_j_next = mbi_reserved_sorted_idx[j + 1];
					if (mbi_reserved[idx_j].baseaddr > mbi_reserved[idx_j_next].baseaddr) {
						mbi_reserved_sorted_idx[j] = idx_j_next;
						mbi_reserved_sorted_idx[j + 1] = idx_j;
					}
				}
			}

			i = 0U;
			for (e820_idx = 0U; e820_idx < e820_entries_count; e820_idx++) {
				uint64_t baseaddr = mmap[e820_idx].baseaddr;
				uint64_t length = mmap[e820_idx].length;

				if (mmap[e820_idx].type != E820_TYPE_RAM) {
					e820[i].baseaddr = baseaddr;
					e820[i].length = length;
					e820[i].type = mmap[e820_idx].type;
					i++;
				} else {
					/* Split this entry to avoid overwrting mbi */
					for (j = 0U; j < 3U; j++) {
						struct e820_entry *mbi_entry = &mbi_reserved[mbi_reserved_sorted_idx[j]];
						uint64_t mbi_baseaddr = mbi_entry->baseaddr;
						uint64_t mbi_length = mbi_entry->length;

						if (mbi_baseaddr + mbi_length <= baseaddr) {
							continue;
						} else if (mbi_baseaddr >= baseaddr &&
							   mbi_baseaddr + mbi_length <= baseaddr + length) {
							if (mbi_baseaddr - baseaddr > 0UL) {
								e820[i].baseaddr = baseaddr;
								e820[i].length = mbi_baseaddr - baseaddr;
								e820[i].type = E820_TYPE_RAM;
								baseaddr = mbi_baseaddr;
								length -= mbi_baseaddr - baseaddr;
								i++;
							}

							e820[i].baseaddr = baseaddr;
							e820[i].length = mbi_length;
							e820[i].type = E820_TYPE_RESERVED;
							baseaddr += mbi_length;
							length -= mbi_length;
							i++;
						} else if (mbi_baseaddr >= baseaddr + length) {
							break;
						} else {
							panic("mbi info overlaps multiple e820 entries!");
						}
					}

					if (length > 0UL) {
						e820[i].baseaddr = baseaddr;
						e820[i].length = length;
						e820[i].type = E820_TYPE_RAM;
						i++;
					}
				}
			}

			e820_entries_count = i;

			for (i = 0U; i < e820_entries_count; i++) {
				dev_dbg(ACRN_DBG_E820, "mmap table: %d type: 0x%x\n", i, mmap[i].type);
				dev_dbg(ACRN_DBG_E820, "Base: 0x%016llx length: 0x%016llx",
					mmap[i].baseaddr, mmap[i].length);
			}
		}

		obtain_e820_mem_info();
	} else {
		panic("no multiboot info found");
	}
}

uint32_t get_e820_entries_count(void)
{
	return e820_entries_count;
}

const struct e820_entry *get_e820_entry(void)
{
	return e820;
}

const struct e820_mem_params *get_e820_mem_info(void)
{
	return &e820_mem;
}
