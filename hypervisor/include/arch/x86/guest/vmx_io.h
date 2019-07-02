/*
 * Copyright (C) 2019 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef IO_EMUL_H
#define IO_EMUL_H

#include <types.h>

/* Define emulated port IO index */
#define PIC_MASTER_PIO_IDX		0U
#define PIC_SLAVE_PIO_IDX		(PIC_MASTER_PIO_IDX + 1U)
#define PIC_ELC_PIO_IDX			(PIC_SLAVE_PIO_IDX + 1U)
#define PCI_CFGADDR_PIO_IDX		(PIC_ELC_PIO_IDX + 1U)
#define PCI_CFGDATA_PIO_IDX		(PCI_CFGADDR_PIO_IDX + 1U)
/* When MAX_VUART_NUM_PER_VM is larger than 2, UART_PIO_IDXn should also be added here */
#define UART_PIO_IDX0			(PCI_CFGDATA_PIO_IDX + 1U)
#define UART_PIO_IDX1			(UART_PIO_IDX0 + 1U)
#define PM1A_EVT_PIO_IDX		(UART_PIO_IDX1 + 1U)
#define PM1A_CNT_PIO_IDX		(PM1A_EVT_PIO_IDX + 1U)
#define PM1B_EVT_PIO_IDX		(PM1A_CNT_PIO_IDX + 1U)
#define PM1B_CNT_PIO_IDX		(PM1B_EVT_PIO_IDX + 1U)
#define RTC_PIO_IDX			(PM1B_CNT_PIO_IDX + 1U)
#define VIRTUAL_PM1A_CNT_PIO_IDX	(RTC_PIO_IDX + 1U)
#define KB_PIO_IDX			(VIRTUAL_PM1A_CNT_PIO_IDX + 1U)
#define CF9_PIO_IDX			(KB_PIO_IDX + 1U)
#define PIO_RESET_REG_IDX		(CF9_PIO_IDX + 1U)
#define EMUL_PIO_IDX_MAX		(PIO_RESET_REG_IDX + 1U)

/**
 * @brief The handler of VM exits on I/O instructions
 *
 * @param vcpu The virtual CPU which triggers the VM exit on I/O instruction
 */
int32_t pio_instr_vmexit_handler(struct acrn_vcpu *vcpu);

/**
 * @brief General complete-work for port I/O emulation
 *
 * @pre io_req->io_type == REQ_PORTIO
 *
 * @remark This function must be called when \p io_req is completed, after
 * either a previous call to emulate_io() returning 0 or the corresponding VHM
 * request having transferred to the COMPLETE state.
 */
void emulate_pio_complete(struct acrn_vcpu *vcpu, const struct io_request *io_req);

#endif /* IO_EMUL_H */
