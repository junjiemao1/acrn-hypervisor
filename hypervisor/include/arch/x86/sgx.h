/*
 * Copyright (C) 2019 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef SGX_H
#define SGX_H

/**
 * @file sgx.h
 *
 * @brief public APIs for SGX
 */

/**
 * @brief SGX
 *
 * @defgroup acrn_sgx ACRN SGX
 * @{
 */

struct epc_section
{
	uint64_t base;	/* EPC section base, must be page aligned */
	uint64_t size;  /* EPC section size in byte, must be page aligned */
};

struct epc_map
{
};

/**
  * @}
  */

#endif
