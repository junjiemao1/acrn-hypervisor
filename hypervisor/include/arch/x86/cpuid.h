/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/*
* cpuid.h
*
*  Created on: Jan 4, 2018
*      Author: don
*/

#ifndef CPUID_H_
#define CPUID_H_

/* CPUID bit definitions */
#define CPUID_ECX_SSE3	  (1U<<0U)
#define CPUID_ECX_DTES64	(1U<<2U)
#define CPUID_ECX_MONITOR       (1U<<3U)
#define CPUID_ECX_DS_CPL	(1U<<4U)
#define CPUID_ECX_VMX	   (1U<<5U)
#define CPUID_ECX_SMX	   (1U<<6U)
#define CPUID_ECX_SDBG	  (1U<<11U)
#define CPUID_ECX_PDCM	  (1U<<15U)
#define CPUID_ECX_XSAVE	 (1U<<26U)
#define CPUID_ECX_OSXSAVE       (1U<<27U)
#define CPUID_ECX_HV	    (1U<<31U)
#define CPUID_EDX_MTRR	  (1U<<12U)
#define CPUID_EDX_DTES	  (1U<<21U)
/* CPUID.07H:EBX.SGX */
#define CPUID_EBX_SGX	   (1U<<2U)
/* CPUID.07H:EBX.MPX */
#define CPUID_EBX_MPX	   (1U<<14U)
/* CPUID.07H:ECX.SGX_LC*/
#define CPUID_ECX_SGX_LC	(1U<<30U)
/* CPUID.07H:EBX.INVPCID*/
#define CPUID_EBX_INVPCID       (1U<<10U)
/* CPUID.07H:EBX.PQM */
#define CPUID_EBX_PQM	   (1U<<12U)
/* CPUID.07H:EBX.PQE */
#define CPUID_EBX_PQE	   (1U<<15U)
/* CPUID.07H:EBX.INTEL_PROCESSOR_TRACE */
#define CPUID_EBX_PROC_TRC      (1U<<25U)
/* CPUID.01H:ECX.PCID*/
#define CPUID_ECX_PCID	  (1U<<17U)
/* CPUID.0DH.EAX.XCR0_BNDREGS */
#define CPUID_EAX_XCR0_BNDREGS  (1U<<3U)
/* CPUID.0DH.EAX.XCR0_BNDCSR */
#define CPUID_EAX_XCR0_BNDCSR   (1U<<4U)
/* CPUID.80000001H.EDX.XD_BIT_AVAILABLE */
#define CPUID_EDX_XD_BIT_AVIL   (1U<<20U)

/* CPUID source operands */
#define CPUID_VENDORSTRING      0U
#define CPUID_FEATURES	  1U
#define CPUID_EXTEND_FEATURE    7U
#define CPUID_MAX_EXTENDED_FUNCTION  0x80000000U
#define CPUID_EXTEND_FUNCTION_1      0x80000001U
#define CPUID_EXTEND_FUNCTION_2      0x80000002U
#define CPUID_EXTEND_FUNCTION_3      0x80000003U
#define CPUID_EXTEND_FUNCTION_4      0x80000004U
#define CPUID_EXTEND_ADDRESS_SIZE    0x80000008U

static inline void asm_cpuid(uint32_t *eax, uint32_t *ebx,
				uint32_t *ecx, uint32_t *edx)
{
	/* Execute CPUID instruction and save results */
	asm volatile("cpuid":"=a"(*eax), "=b"(*ebx),
			"=c"(*ecx), "=d"(*edx)
			: "0" (*eax), "2" (*ecx)
			: "memory");
}

static inline void cpuid(uint32_t leaf,
			uint32_t *eax, uint32_t *ebx,
			uint32_t *ecx, uint32_t *edx)
{
	*eax = leaf;
	*ecx = 0U;

	asm_cpuid(eax, ebx, ecx, edx);
}

static inline void cpuid_subleaf(uint32_t leaf, uint32_t subleaf,
				uint32_t *eax, uint32_t *ebx,
				uint32_t *ecx, uint32_t *edx)
{
	*eax = leaf;
	*ecx = subleaf;

	asm_cpuid(eax, ebx, ecx, edx);
}

#endif /* CPUID_H_ */
