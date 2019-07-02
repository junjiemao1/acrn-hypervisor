/*-
 * Copyright (c) 1996, by Peter Wemm and Steve Passe
 * Copyright (c) 2017 Intel Corporation
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. The name of the developer may NOT be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
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

#ifndef APICREG_H
#define APICREG_H

#include <page.h>

/*
 * Local && I/O APIC definitions.
 */

/******************************************************************************
 * global defines, etc.
 */

/******************************************************************************
 * LOCAL APIC structure
 */
struct lapic_reg {
	uint32_t v;
};

struct lapic_regs {			 /*OFFSET(Hex)*/
	struct lapic_reg	rsv0[2];
	struct lapic_reg	id;	  /*020*/
	struct lapic_reg	version;  /*030*/
	struct lapic_reg	rsv1[4];
	struct lapic_reg	ppr;	  /*0A0*/
	struct lapic_reg	ldr;	  /*0D0*/
	struct lapic_reg	dfr;	  /*0EO*/
	struct lapic_reg	svr;	  /*0F0*/
	struct lapic_reg	tmr[8];	  /*180 -- 1F0*/
	struct lapic_reg	rsv2[6];
	struct lapic_reg	lvt_cmci; /*2F0*/
	struct lapic_reg	lvt[6];	  /*320 -- 370*/
	struct lapic_reg	icr_timer;/*380*/
	struct lapic_reg	rsv3[4];
	struct lapic_reg	dcr_timer;/*3E0*/

	/*roundup sizeof current struct to 4KB*/
	struct lapic_reg	rsv5[192]; /*400 -- FF0*/
} __aligned(PAGE_SIZE);

/******************************************************************************
 * I/O APIC structure
 */

/*
 * Macros for bits in union ioapic_rte
 */
#define IOAPIC_RTE_MASK_CLR		0x0U	/* Interrupt Mask: Clear */
#define IOAPIC_RTE_MASK_SET		0x1U	/* Interrupt Mask: Set */

#define IOAPIC_RTE_TRGRMODE_EDGE	0x0U	/* Trigger Mode: Edge */
#define IOAPIC_RTE_TRGRMODE_LEVEL	0x1U	/* Trigger Mode: Level */

#define IOAPIC_RTE_INTPOL_AHI		0x0U	/* Interrupt Polarity: active high */

#define IOAPIC_RTE_DESTMODE_LOGICAL	0x1U	/* Destination Mode: Logical */

#define IOAPIC_RTE_DELMODE_LOPRI	0x1U	/* Delivery Mode: Lowest priority */

/* IOAPIC Redirection Table (RTE) Entry structure */
union ioapic_rte {
	uint64_t full;
	struct {
		uint32_t lo_32;
		uint32_t hi_32;
	} u;
	struct {
		uint64_t vector:8;
		uint64_t delivery_mode:3;
		uint64_t dest_mode:1;
		uint64_t delivery_status:1;
		uint64_t intr_polarity:1;
		uint64_t remote_irr:1;
		uint64_t trigger_mode:1;
		uint64_t intr_mask:1;
		uint64_t rsvd_1:39;
		uint64_t dest_field:8;
	} bits __packed;
};

/******************************************************************************
 * various code 'logical' values
 */

/******************************************************************************
 * LOCAL APIC defines
 */

/* default physical locations of LOCAL (CPU) APICs */
#define DEFAULT_APIC_BASE	0xfee00000UL

/* constants relating to APIC ID registers */
#define APIC_ID_MASK		0xff000000U
#define	APIC_ID_SHIFT		24U

#define MAXLVTSHIFT		16U

#define	APIC_DFR_MODEL_MASK	0xf0000000U
#define	APIC_DFR_MODEL_FLAT	0xf0000000U
#define	APIC_DFR_MODEL_CLUSTER	0x00000000U

/* fields in SVR */
#define APIC_SVR_VECTOR		0x000000ffU
#define APIC_SVR_ENABLE		0x00000100U

/* fields in ICR_LOW */
#define APIC_VECTOR_MASK	0x000000ffU

#define APIC_DELMODE_MASK	0x00000700U
#define APIC_DELMODE_INIT	0x00000500U
#define APIC_DELMODE_STARTUP	0x00000600U

#define APIC_DESTMODE_LOG	0x00000800U

#define APIC_LEVEL_MASK		0x00004000U
#define APIC_LEVEL_DEASSERT	0x00000000U

#define APIC_DEST_MASK		0x000c0000U
#define APIC_DEST_DESTFLD	0x00000000U

/* fields in LVT1/2 */
#define APIC_LVT_VECTOR		0x000000ffU
#define APIC_LVT_DM		0x00000700U
#define APIC_LVT_DS		0x00001000U
#define APIC_LVT_IIPP		0x00002000U
#define APIC_LVT_RIRR		0x00004000U
#define APIC_LVT_TM		0x00008000U
#define APIC_LVT_M		0x00010000U

#define APIC_LVTT_TM		0x00060000U
#define APIC_LVTT_TM_PERIODIC	0x00020000U

/* LVT table indices */
#define	APIC_LVT_TIMER		0U
#define	APIC_LVT_THERMAL	1U
#define	APIC_LVT_PMC		2U
#define	APIC_LVT_LINT0		3U
#define	APIC_LVT_LINT1		4U
#define	APIC_LVT_ERROR		5U
#define	APIC_LVT_CMCI		6U

/******************************************************************************
 * I/O APIC defines
 */

/* window register offset */
#define IOAPIC_REGSEL		0x00U
#define IOAPIC_WINDOW		0x10U

#define IOAPIC_VER		0x01U

/* fields in VER, for redirection entry */
#define IOAPIC_MAX_RTE_MASK	0x00ff0000U
#define MAX_RTE_SHIFT		16U

#endif /* APICREG_H */
