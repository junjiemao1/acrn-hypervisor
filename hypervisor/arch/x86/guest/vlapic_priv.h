/*-
 * Copyright (c) 2013 Neel Natu <neel@freebsd.org>
 * Copyright (c) 2017 Intel Corporation
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY NETAPP, INC ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL NETAPP, INC OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#ifndef VLAPIC_PRIV_H
#define VLAPIC_PRIV_H

/*
 * APIC Register:		Offset	Description
 */
#define APIC_OFFSET_ID		0x20U	/* Local APIC ID		*/
#define APIC_OFFSET_LDR		0xD0U	/* Logical Destination		*/
#define APIC_OFFSET_CMCI_LVT	0x2F0U	/* Local Vector Table (CMCI)	*/
#define APIC_OFFSET_TIMER_LVT	0x320U	/* Local Vector Table (Timer)	*/
#define APIC_OFFSET_THERM_LVT	0x330U	/* Local Vector Table (Thermal)	*/
#define APIC_OFFSET_PERF_LVT	0x340U	/* Local Vector Table (PMC)	*/
#define APIC_OFFSET_LINT0_LVT	0x350U	/* Local Vector Table (LINT0)	*/
#define APIC_OFFSET_LINT1_LVT	0x360U	/* Local Vector Table (LINT1)	*/
#define APIC_OFFSET_ERROR_LVT	0x370U	/* Local Vector Table (ERROR)	*/

struct acrn_apicv_ops {
	bool (*inject_intr)(struct acrn_vlapic *vlapic, bool guest_irq_enabled, bool injected);
	bool (*has_pending_delivery_intr)(struct acrn_vcpu *vcpu);
};

#endif /* VLAPIC_PRIV_H */
