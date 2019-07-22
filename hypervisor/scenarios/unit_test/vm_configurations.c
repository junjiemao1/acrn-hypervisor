/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <vm_config.h>
#include <vuart.h>

extern struct acrn_vm_pci_ptdev_config vm0_pci_ptdevs[VM0_CONFIG_PCI_PTDEV_NUM];
extern struct acrn_vm_pci_ptdev_config vm1_pci_ptdevs[VM1_CONFIG_PCI_PTDEV_NUM];

struct acrn_vm_config vm_configs[CONFIG_MAX_VM_NUM] = {
	{	/* VM0 */
		.load_order = PRE_LAUNCHED_VM,
		.name = "ACRN UNIT TEST",
		.uuid = {0x26U, 0xc5U, 0xe0U, 0xd8U, 0x8fU, 0x8aU, 0x47U, 0xd8U,	\
			 0x81U, 0x09U, 0xf2U, 0x01U, 0xebU, 0xd6U, 0x1aU, 0x5eU},
			/* 26c5e0d8-8f8a-47d8-8109-f201ebd61a5e */
		.pcpu_bitmap = VM0_CONFIG_PCPU_BITMAP,
		.clos = 0U,
		.memory = {
			.start_hpa = VM0_CONFIG_MEM_START_HPA,
			.size = VM0_CONFIG_MEM_SIZE,
		},
		.os_config = {
			.name = "ACRN unit test",
			.kernel_type = KERNEL_BZIMAGE,
			.kernel_mod_tag = "Linux_bzImage",
			.bootargs = "help"
		},
		.vuart[0] = {
			.type = VUART_LEGACY_PIO,
			.addr.port_base = COM1_BASE,
			.irq = COM1_IRQ,
		},
		.pci_ptdev_num = VM0_CONFIG_PCI_PTDEV_NUM,
		.pci_ptdevs = vm0_pci_ptdevs,
	},
};
