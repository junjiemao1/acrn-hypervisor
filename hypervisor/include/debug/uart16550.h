/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef UART16550_H
#define UART16550_H

/* Register / bit definitions for 16c550 uart */

/*enable/disable receive data read request interrupt*/

/* bit definitions for LSR */

bool is_pci_dbg_uart(union pci_bdf bdf_value);

#endif /* !UART16550_H */
