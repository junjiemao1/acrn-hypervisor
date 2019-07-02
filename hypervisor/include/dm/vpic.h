/*-
 * Copyright (c) 2014 Tycho Nightingale <tycho.nightingale@pluribusnetworks.com>
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND
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

#ifndef VPIC_H
#define VPIC_H

/**
 * @file vpic.h
 *
 * @brief public APIs for virtual PIC
 */

/* 0x20 - 0x80 - in 8080/8085 mode only */

/* Initialization control word 2. Written to the odd address. */
/* No definitions, it is the base vector of the IDT for 8086 mode */

/* Initialization control word 3. Written to the odd address. */
/* For a master PIC, bitfield indicating a slave 8259 on given input */
/* For slave, lower 3 bits are the slave's ID binary id on master */

/* Operation control words.  Written after initialization. */

/* Operation control word type 1 */
/*
 * No definitions.  Written to the odd address.  Bitmask for interrupts.
 * 1 = disabled.
 */

/* 0x08 must be 0 to select OCW2 vs OCW3 */

enum vpic_wire_mode {
	VPIC_WIRE_INTR = 0,
	VPIC_WIRE_LAPIC,
	VPIC_WIRE_IOAPIC,
	VPIC_WIRE_NULL
};

/**
 * @brief virtual PIC
 *
 * @addtogroup acrn_vpic ACRN vPIC
 * @{
 */

/**
 * @}
 */
/* End of acrn_vpic */

#endif /* VPIC_H */
