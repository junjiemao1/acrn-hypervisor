/*
 * Copyright (C) 2019 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <types.h>
#include <cpu.h>
#include <cpu_caps.h>
#include <cpufeatures.h>
#include <cpuid.h>
#include <errno.h>
#include <logmsg.h>
#include <cat.h>
#include <board.h>

struct cat_hw_info cat_cap_info;

void setup_clos(uint16_t pcpu_id)
{
	uint16_t i;
	uint32_t msr_index;
	uint64_t val;

	if (cat_cap_info.enabled) {
		for (i = 0U; i < platform_clos_num; i++) {
			msr_index = platform_clos_array[i].msr_index;
			val = (uint64_t)platform_clos_array[i].clos_mask;
			msr_write_pcpu(msr_index, val, pcpu_id);
		}
	}
}
