/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/* Given the reality that some of ACPI configrations are unlikey changed,
 * define these MACROs in this header file.
 * The platform_acpi_info.h still has chance to override the default
 * definition by #undef with offline tool.
 */
#ifndef DEFAULT_ACPI_INFO_H
#define DEFAULT_ACPI_INFO_H

/* APIC */
#define LAPIC_BASE		0xFEE00000UL

#endif	/* DEFAULT_ACPI_INFO_H */
