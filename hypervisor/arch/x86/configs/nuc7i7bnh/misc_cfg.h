/*
 * Copyright (C) 2019 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef MISC_CFG_H
#define MISC_CFG_H

#define ROOTFS_0		"root=/dev/sda3 "

#ifndef CONFIG_RELEASE
#define SOS_BOOTARGS_DIFF	"hvlog=2M@0x1FE00000"
#else
#endif

#endif /* MISC_CFG_H */
