/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/**
 * @file hypercall.h
 *
 * @brief public APIs for hypercall
 */

#ifndef HYPERCALL_H
#define HYPERCALL_H

/**
 * @brief Hypercall
 *
 * @addtogroup acrn_hypercall ACRN Hypercall
 * @{
 */

/**
 * @brief Get VCPU Power state.
 *
 * @param vm pointer to VM data structure
 * @param cmd cmd to show get which VCPU power state data
 * @param param VCPU power state data
 *
 * @pre Pointer vm shall point to SOS_VM
 * @return 0 on success, non-zero on error.
 */

/**
 * @defgroup trusty_hypercall Trusty Hypercalls
 *
 * This is a special group that includes all hypercalls
 * related to Trusty
 *
 * @{
 */

/**
 * @brief Switch vCPU state between Normal/Secure World.
 *
 * * The hypervisor uses this hypercall to do the world switch
 * * The hypervisor needs to:
 *   * save current world vCPU contexts, and load the next world
 *     vCPU contexts
 *   * update ``rdi``, ``rsi``, ``rdx``, ``rbx`` to next world
 *     vCPU contexts
 *
 * @param vcpu Pointer to VCPU data structure
 *
 * @return 0 on success, non-zero on error.
 */

/**
 * @}
 */
/* End of trusty_hypercall */

/**
 * @}
 */
/* End of acrn_hypercall */

#endif /* HYPERCALL_H*/
