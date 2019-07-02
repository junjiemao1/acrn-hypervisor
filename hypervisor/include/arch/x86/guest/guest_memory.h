/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
/**
 * @file guest_memory.h
 *
 * @brief ACRN Memory Management
 */
#ifndef GUEST_H
#define GUEST_H

#ifndef ASSEMBLER

#include <types.h>

struct acrn_vcpu;
struct acrn_vm;

/* gpa --> hpa -->hva */
void *gpa2hva(struct acrn_vm *vm, uint64_t x);

/**
 * @brief Data transfering between hypervisor and VM
 *
 * @defgroup acrn_mem ACRN Memory Management
 * @{
 */
/**
 * @brief Copy data from HV address space to VM GPA space
 *
 * @param[in] vm The pointer that points to VM data structure
 * @param[in] h_ptr The pointer that points the start HV address
 *		  of HV memory region which data is stored in
 * @param[out] gpa The start GPA address of GPA memory region which data
 *		 will be copied into
 * @param[in] size The size (bytes) of GPA memory region which data will be
 *		 copied into
 *
 * @pre Caller(Guest) should make sure gpa is continuous.
 * - gpa from hypercall input which from kernel stack is gpa continuous, not
 *   support kernel stack from vmap
 * - some other gpa from hypercall parameters, VHM should make sure it's
 *   continuous
 * @pre Pointer vm is non-NULL
 */
int32_t copy_to_gpa(struct acrn_vm *vm, void *h_ptr, uint64_t gpa, uint32_t size);
/**
 * @}
 */
#endif	/* !ASSEMBLER */

#endif /* GUEST_H*/
