/*
 * hypercall definition
 *
 * Copyright (C) 2017 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/**
 * @file acrn_hv_defs.h
 *
 * @brief acrn data structure for hypercall
 */

#ifndef ACRN_HV_DEFS_H
#define ACRN_HV_DEFS_H

/*
 * Common structures for HV/VHM
 */

#define ACRN_INVALID_HPA (~0UL)

/**
 * @brief Hypercall
 *
 * @defgroup acrn_hypercall ACRN Hypercall
 * @{
 */

#define MR_DEL		2U
#define MR_MODIFY	3U

/**
 * @}
 */

#endif /* ACRN_HV_DEFS_H */
