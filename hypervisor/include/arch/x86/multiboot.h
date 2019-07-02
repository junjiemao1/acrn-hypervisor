/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef MULTIBOOT_H
#define MULTIBOOT_H

#include <types.h>
#define	MULTIBOOT_INFO_MAGIC		0x2BADB002U
#define	MULTIBOOT_INFO_HAS_MODS		0x00000008U
#define	MULTIBOOT_INFO_HAS_MMAP		0x00000040U

/* maximum lengt of the guest OS' command line parameter string */
#define MAX_BOOTARGS_SIZE		2048U

struct acrn_vm;
struct multiboot_info {
	uint32_t	       mi_flags;

	/* Valid if mi_flags sets MULTIBOOT_INFO_HAS_MODS. */
	uint32_t	       mi_mods_count;
	uint32_t	       mi_mods_addr;

	/* Valid if mi_flags sets MULTIBOOT_INFO_HAS_MMAP. */
	uint32_t	       mi_mmap_length;
	uint32_t	       mi_mmap_addr;

	/* Valid if mi_flags sets MULTIBOOT_INFO_HAS_LOADER_NAME. */
	uint32_t	       mi_loader_name;

};

struct multiboot_mmap {
	uint64_t baseaddr;
	uint64_t length;
	uint32_t type;
} __packed;

struct multiboot_module {
	uint32_t	mm_mod_start;
	uint32_t	mm_mod_end;
	uint32_t	mm_string;
};

/* boot_regs store the multiboot header address */
extern uint32_t boot_regs[2];
#endif
