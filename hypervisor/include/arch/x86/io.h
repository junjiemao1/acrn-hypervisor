/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef IO_H
#define IO_H

#include <types.h>

/* Write 1 byte to specified I/O port */
static inline void pio_write8(uint8_t value, uint16_t port)
{
	asm volatile ("outb %0,%1"::"a" (value), "dN"(port));
}

/* Read 1 byte from specified I/O port */
static inline uint8_t pio_read8(uint16_t port)
{
	uint8_t value;

	asm volatile ("inb %1,%0":"=a" (value):"dN"(port));
	return value;
}

/* Write 2 bytes to specified I/O port */
static inline void pio_write16(uint16_t value, uint16_t port)
{
	asm volatile ("outw %0,%1"::"a" (value), "dN"(port));
}

/* Read 2 bytes from specified I/O port */
static inline uint16_t pio_read16(uint16_t port)
{
	uint16_t value;

	asm volatile ("inw %1,%0":"=a" (value):"dN"(port));
	return value;
}

/* Write 4 bytes to specified I/O port */
static inline void pio_write32(uint32_t value, uint16_t port)
{
	asm volatile ("outl %0,%1"::"a" (value), "dN"(port));
}

/* Read 4 bytes from specified I/O port */
static inline uint32_t pio_read32(uint16_t port)
{
	uint32_t value;

	asm volatile ("inl %1,%0":"=a" (value):"dN"(port));
	return value;
}

/** Writes a 32 bit value to a memory mapped IO device.
 *
 *  @param value The 32 bit value to write.
 *  @param addr The memory address to write to.
 */
static inline void mmio_write32(uint32_t value, void *addr)
{
	volatile uint32_t *addr32 = (volatile uint32_t *)addr;
	*addr32 = value;
}

/** Reads a 32 bit value from a memory mapped IO device.
 *
 *  @param addr The memory address to read from.
 *
 *  @return The 32 bit value read from the given address.
 */
static inline uint32_t mmio_read32(const void *addr)
{
	return *((volatile const uint32_t *)addr);
}

#endif /* _IO_H defined */
