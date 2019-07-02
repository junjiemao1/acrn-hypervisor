/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef ACPI_H
#define ACPI_H

#include <vm_configurations.h>

struct acpi_table_header {
	/* ASCII table signature */
	char		    signature[4];
	/* Length of table in bytes, including this header */
	uint32_t		length;
};

void *get_acpi_tbl(const char *signature);

struct ioapic_info;
uint16_t parse_madt(uint32_t lapic_id_array[CONFIG_MAX_PCPU_NUM]);
uint16_t parse_madt_ioapic(struct ioapic_info *ioapic_id_array);

#ifdef CONFIG_ACPI_PARSE_ENABLED
void acpi_fixup(void);
#endif

#endif /* !ACPI_H */
