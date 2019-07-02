/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef RTL_H
#define RTL_H

#include <types.h>

int32_t strncmp(const char *s1_arg, const char *s2_arg, size_t n_arg);
size_t strnlen_s(const char *str_arg, size_t maxlen_arg);
void *memset(void *base, uint8_t v, size_t n);
void *memcpy_s(void *d, size_t dmax, const void *s, size_t slen);

#endif /* RTL_H */
