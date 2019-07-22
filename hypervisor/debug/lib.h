/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef LIB_H
#define LIB_H

#include <vcpu.h>
#include <vm.h>

union u_qword {
	struct {
		uint32_t low;
		uint32_t high;
	} dwords;

	uint64_t qword;

};

typedef signed char int8_t;
typedef signed long int64_t;

/* MACRO related to string */
#define ULONG_MAX	((uint64_t)(~0UL))	/* 0xFFFFFFFF */
#define LONG_MAX	(ULONG_MAX >> 1U)	/* 0x7FFFFFFF */
#define LONG_MIN	(~LONG_MAX)		/* 0x80000000 */

#define va_start	__builtin_va_start
#define va_end		__builtin_va_end
typedef __builtin_va_list va_list;

void printf(const char *fmt, ...);
size_t vsnprintf(char *dst_arg, size_t sz_arg, const char *fmt, va_list args);
size_t snprintf(char *dest, size_t sz, const char *fmt, ...);

int64_t strtol_deci(const char *nptr);
uint64_t strtoul_hex(const char *nptr);
int32_t strcmp(const char *s1_arg, const char *s2_arg);
char *strncpy_s(char *d_arg, size_t dmax, const char *s_arg, size_t slen_arg);
char *strchr(char *s_arg, char ch);

static inline bool is_space(char c)
{
	return ((c == ' ') || (c == '\t'));
}

#define ACRN_INVALID_VMID (0xffffU)

size_t console_write(const char *s, size_t len);
void console_putc(const char *ch);
char console_getc(void);

void shell_kick(void);

static inline uint64_t vcpu_get_cr2(const struct acrn_vcpu *vcpu)
{
	return vcpu->arch.contexts[vcpu->arch.cur_context].run_ctx.cr2;
}

void register_testdev(struct acrn_vm *vm);

#endif/* LIB_H */
