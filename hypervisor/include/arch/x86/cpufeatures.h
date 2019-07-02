/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef CPUFEATURES_H
#define CPUFEATURES_H

#define X86_FEATURE_MONITOR	((FEAT_1_ECX << 5U) +  3U)
#define X86_FEATURE_VMX		((FEAT_1_ECX << 5U) +  5U)
#define X86_FEATURE_X2APIC	((FEAT_1_ECX << 5U) + 21U)
#define X86_FEATURE_TSC_DEADLINE	((FEAT_1_ECX << 5U) + 24U)
#define X86_FEATURE_XSAVE	((FEAT_1_ECX << 5U) + 26U)
#define X86_FEATURE_OSXSAVE	((FEAT_1_ECX << 5U) + 27U)

#define X86_FEATURE_MTRR	((FEAT_1_EDX << 5U) + 12U)

#define X86_FEATURE_SMEP	((FEAT_7_0_EBX << 5U) +  7U)
#define X86_FEATURE_ERMS	((FEAT_7_0_EBX << 5U) +  9U)
#define X86_FEATURE_SMAP	((FEAT_7_0_EBX << 5U) + 20U)
#define X86_FEATURE_CLFLUSHOPT	((FEAT_7_0_EBX << 5U) + 23U)

/* Intel-defined CPU features, CPUID level 0x00000007 (EDX)*/
#define X86_FEATURE_IBRS_IBPB	((FEAT_7_0_EDX << 5U) + 26U)
#define X86_FEATURE_STIBP	((FEAT_7_0_EDX << 5U) + 27U)
#define X86_FEATURE_L1D_FLUSH	((FEAT_7_0_EDX << 5U) + 28U)
#define X86_FEATURE_ARCH_CAP	((FEAT_7_0_EDX << 5U) + 29U)

/* Intel-defined CPU features, CPUID level 0x80000001 (EDX)*/
#define X86_FEATURE_NX		((FEAT_8000_0001_EDX << 5U) + 20U)
#define X86_FEATURE_PAGE1GB	((FEAT_8000_0001_EDX << 5U) + 26U)
#define X86_FEATURE_LM		((FEAT_8000_0001_EDX << 5U) + 29U)

#endif /* CPUFEATURES_H */
