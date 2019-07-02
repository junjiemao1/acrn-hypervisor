/*
 * common definition
 *
 * Copyright (C) 2017 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/**
 * @file acrn_common.h
 *
 * @brief acrn common data structure for hypercall or ioctl
 */

#ifndef ACRN_COMMON_H
#define ACRN_COMMON_H

#include <types.h>

/*
 * Common structures for ACRN/VHM/DM
 */

#define REQ_PORTIO	0U

#define REQUEST_READ	0U
#define REQUEST_WRITE	1U

/* IOAPIC device model info */
#define VIOAPIC_RTE_NUM	48U  /* vioapic pins */

#if VIOAPIC_RTE_NUM < 24U
#error "VIOAPIC_RTE_NUM must be larger than 23"
#endif

/* Generic VM flags from guest OS */
#define GUEST_FLAG_SECURE_WORLD_ENABLED		(1UL << 0U)	/* Whether secure world is enabled */
#define GUEST_FLAG_LAPIC_PASSTHROUGH		(1UL << 1U)  	/* Whether LAPIC is passed through */
#define GUEST_FLAG_CLOS_REQUIRED		(1UL << 3U)     /* Whether CLOS is required */
#define GUEST_FLAG_RT				(1UL << 5U)     /* Whether the vm is RT-VM */

/**
 * @brief Hypercall
 *
 * @addtogroup acrn_hypercall ACRN Hypercall
 * @{
 */

/**
 * @brief Representation of a MMIO request
 */
struct mmio_request {
	/**
	 * @brief Direction of the access
	 *
	 * Either \p REQUEST_READ or \p REQUEST_WRITE.
	 */
	uint32_t direction;

	/**
	 * @brief Width of the I/O access in byte
	 */
	uint64_t size;

	/**
	 * @brief The value read for I/O reads or to be written for I/O writes
	 */
	uint64_t value;
} __aligned(8);

/**
 * @brief Representation of a port I/O request
 */
struct pio_request {
	/**
	 * @brief Direction of the access
	 *
	 * Either \p REQUEST_READ or \p REQUEST_WRITE.
	 */
	uint32_t direction;

	/**
	 * @brief Port address of the I/O access
	 */
	uint64_t address;

	/**
	 * @brief Width of the I/O access in byte
	 */
	uint64_t size;

	/**
	 * @brief The value read for I/O reads or to be written for I/O writes
	 */
	uint32_t value;
} __aligned(8);

union vhm_io_request {
	struct pio_request pio;
	struct mmio_request mmio;
};

/* General-purpose register layout aligned with the general-purpose register idx
 * when vmexit, such as vmexit due to CR access, refer to SMD Vol.3C 27-6.
 */
struct acrn_gp_regs {
	uint64_t rax;
	uint64_t rcx;
	uint64_t rdx;
	uint64_t rbx;
	uint64_t rsp;
	uint64_t rbp;
	uint64_t rsi;
	uint64_t rdi;
	uint64_t r8;
	uint64_t r9;
	uint64_t r10;
	uint64_t r11;
	uint64_t r12;
	uint64_t r13;
	uint64_t r14;
	uint64_t r15;
};

/* struct to define how the descriptor stored in memory.
 * Refer SDM Vol3 3.5.1 "Segment Descriptor Tables"
 * Figure 3-11
 */
struct acrn_descriptor_ptr {
	uint16_t limit;
	uint64_t base;
	uint16_t reserved[3];   /* align struct size to 64bit */
} __packed;

/**
 * @brief registers info for vcpu.
 */
struct acrn_vcpu_regs {
	struct acrn_gp_regs gprs;
	struct acrn_descriptor_ptr gdt;
	struct acrn_descriptor_ptr idt;

	uint64_t	rip;
	uint64_t	cs_base;
	uint64_t	cr0;
	uint64_t	cr4;
	uint64_t	cr3;
	uint64_t	ia32_efer;
	uint64_t	rflags;
	uint64_t	reserved_64[4];

	uint32_t	cs_ar;
	uint32_t	cs_limit;
	uint32_t	reserved_32[3];

	/* don't change the order of following sel */
	uint16_t	cs_sel;
	uint16_t	ss_sel;
	uint16_t	ds_sel;
	uint16_t	es_sel;
	uint16_t	fs_sel;
	uint16_t	gs_sel;
	uint16_t	ldt_sel;
	uint16_t	tr_sel;

	uint16_t	reserved_16[4];
};

/**
 * @brief Info The power state data of a VCPU.
 *
 */

/**
 * @}
 */
#endif /* ACRN_COMMON_H */
