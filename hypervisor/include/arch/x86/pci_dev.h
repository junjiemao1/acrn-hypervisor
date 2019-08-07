/*
 * Copyright (C) 2019 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef PCI_DEV_H_
#define PCI_DEV_H_

#include <vm_config.h>

#define SOS_EMULATED_PCI_DEV_NUM	1U

struct pci_pdev;
void fill_pci_dev_config(struct pci_pdev *pdev);

#endif /* PCI_DEV_H_ */
