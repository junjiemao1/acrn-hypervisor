/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef TRUSTY_H_
#define TRUSTY_H_
#include <acrn_hv_defs.h>
#include <seed.h>

#define TRUSTY_RAM_SIZE	(16UL * 1024UL * 1024UL)	/* 16 MB for now */

/* Trusty EPT rebase gpa: 511G */
#define TRUSTY_EPT_REBASE_GPA (511UL * 1024UL * 1024UL * 1024UL)

struct acrn_vcpu;
struct acrn_vm;

struct secure_world_control {
	/* Flag indicates Secure World's state */
	struct {
		/* sworld supporting: 0(unsupported), 1(supported) */
		uint64_t supported :  1;
		/* sworld running status: 0(inactive), 1(active) */
		uint64_t active    :  1;
		/* sworld context saving status: 0(unsaved), 1(saved) */
		uint64_t ctx_saved :  1;
		uint64_t reserved  : 61;
	} flag;
};

#endif /* TRUSTY_H_ */
