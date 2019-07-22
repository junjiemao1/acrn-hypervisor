/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <types.h>
#include <pci.h>
#include <shell.h>
#include <timer.h>
#include <irq.h>
#include <logmsg.h>
#include <acrn_hv_defs.h>
#include <vm.h>
#include "vuart.h"
#include "uart16550.h"
#include "lib.h"

#define CONSOLE_KICK_TIMER_TIMEOUT  40UL /* timeout is 40ms*/
/* Switching key combinations for shell and uart console */
#define GUEST_CONSOLE_TO_HV_SWITCH_KEY      0       /* CTRL + SPACE */
uint16_t console_vmid = ACRN_INVALID_VMID;

void console_init(void)
{
	uart16550_init();
}

void console_putc(const char *ch)
{
	(void)uart16550_puts(ch, 1U);
}


size_t console_write(const char *s, size_t len)
{
	return  uart16550_puts(s, len);
}

char console_getc(void)
{
	return uart16550_getc();
}

/**
 * @pre vu != NULL
 * @pre vu->active == true
 */
void vuart_console_rx_chars(struct acrn_vuart *vu)
{
	char ch = -1;

	/* Get data from physical uart */
	ch = uart16550_getc();

	if (ch == GUEST_CONSOLE_TO_HV_SWITCH_KEY) {
		/* Switch the console */
		console_vmid = ACRN_INVALID_VMID;
		printf("\r\n\r\n ---Entering ACRN SHELL---\r\n");
	}
	if (ch != -1) {
		vuart_putchar(vu, ch);
	}

}

/**
 * @pre vu != NULL
 */
void vuart_console_tx_chars(struct acrn_vuart *vu)
{
	char c;

	while ((c = vuart_getchar(vu)) != -1) {
		printf("%c", c);
	}
}

struct acrn_vuart *vuart_console_active(void)
{
	struct acrn_vm *vm = NULL;
	struct acrn_vuart *vu = NULL;

	if (console_vmid < CONFIG_MAX_VM_NUM) {
		vm = get_vm_from_vmid(console_vmid);
		if (vm->state != VM_POWERED_OFF) {
			vu = vm_console_vuart(vm);
		} else {
			/* Console vm is invalid, switch back to HV-Shell */
			console_vmid = ACRN_INVALID_VMID;
		}
	}

	return ((vu != NULL) && vu->active) ? vu : NULL;
}

static void console_timer_callback(__unused uint32_t irq, __unused void *data)
{
	struct acrn_vuart *vu;

	/* Kick HV-Shell and Uart-Console tasks */
	vu = vuart_console_active();
	if (vu != NULL) {
		/* serial Console Rx operation */
		vuart_console_rx_chars(vu);
		/* serial Console Tx operation */
		vuart_console_tx_chars(vu);
	} else {
		shell_kick();
	}

	/* Re-arm the timer */
	msr_write(MSR_IA32_TSC_DEADLINE, rdtsc() + us_to_ticks(10000));
}

#define LAPIC_TMR_TSC_DEADLINE                  ((uint32_t) 0x2U << 17U)

void console_setup_timer(void)
{
	int32_t retval;

	/* Request for timer irq */
	retval = request_irq(TIMER_IRQ, (irq_action_t)console_timer_callback, NULL, IRQF_NONE);
	if (retval >= 0) {
		/* Enable TSC deadline timer mode */
		msr_write(MSR_IA32_EXT_APIC_LVT_TIMER, LAPIC_TMR_TSC_DEADLINE | VECTOR_TIMER);

		/* Arm the timer */
		msr_write(MSR_IA32_TSC_DEADLINE, rdtsc() + us_to_ticks(50000));
	} else {
		pr_err("Timer setup failed. Console is disabled.");
	}
}

void suspend_console(void)
{
	msr_write(MSR_IA32_TSC_DEADLINE, 0UL);
}

void resume_console(void)
{
	msr_write(MSR_IA32_TSC_DEADLINE, rdtsc() + us_to_ticks(5000));
}
