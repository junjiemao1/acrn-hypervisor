/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <types.h>
#include <pci.h>

void console_init(void) {}
void console_setup_timer(void) {}

bool is_pci_dbg_uart(__unused union pci_bdf bdf_value) { return false; }

void shell_init(void) {}
