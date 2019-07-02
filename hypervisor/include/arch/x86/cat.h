/*
 * Copyright (C) 2019 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef CAT_H
#define CAT_H

/* The intel Resource Director Tech(RDT) based Cache Allocation Tech support */
struct cat_hw_info {
	bool support;		/* If L2/L3 CAT supported */
	bool enabled;		/* If any VM setup CLOS */
	uint16_t clos_max;	/* Maximum CLOS supported, the number of cache masks */

};

extern struct cat_hw_info cat_cap_info;
void setup_clos(uint16_t pcpu_id);

#endif	/* CAT_H */
