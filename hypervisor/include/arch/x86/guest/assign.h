/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef ASSIGN_H
#define ASSIGN_H

#include <types.h>
#include <ptdev.h>

/**
 * @file assign.h
 *
 * @brief public APIs for Passthrough Interrupt Remapping
 */

/**
 * @brief VT-d
 *
 * @defgroup acrn_passthrough ACRN Passthrough
 * @{
 */

/**
 * @brief MSI/MSI-x remapping for passthrough device.
 *
 * Main entry for PCI device assignment with MSI and MSI-X.
 * MSI can up to 8 vectors and MSI-X can up to 1024 Vectors.
 *
 * @param[in] vm pointer to acrn_vm
 * @param[in] virt_bdf virtual bdf associated with the passthrough device
 * @param[in] phys_bdf virtual bdf associated with the passthrough device
 * @param[in] entry_nr indicate coming vectors, entry_nr = 0 means first vector
 * @param[in] info structure used for MSI/MSI-x remapping
 *
 * @return
 *    - 0: on success
 *    - \p -ENODEV:
 *      - for SOS, the entry already be held by others
 *      - for UOS, no pre-hold mapping found.
 *
 * @pre vm != NULL
 * @pre info != NULL
 *
 */
int32_t ptirq_msix_remap(struct acrn_vm *vm, uint16_t virt_bdf,  uint16_t phys_bdf,
				uint16_t entry_nr, struct ptirq_msi_info *info);

/**
 * @brief Remove interrupt remapping entry/entries for MSI/MSI-x.
 *
 * Remove the mapping of given number of vectors of the given virtual BDF for the given vm.
 *
 * @param[in] vm pointer to acrn_vm
 * @param[in] virt_bdf virtual bdf associated with the passthrough device
 * @param[in] vector_count number of vectors
 *
 * @return None
 *
 * @pre vm != NULL
 *
 */
void ptirq_remove_msix_remapping(const struct acrn_vm *vm, uint16_t virt_bdf, uint32_t vector_count);

/**
  * @}
  */

#endif /* ASSIGN_H */
