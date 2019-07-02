/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef CONSOLE_H
#define CONSOLE_H

#include <vuart.h>

/** Initializes the console module.
 *
 */
void console_init(void);

void console_setup_timer(void);

#endif /* CONSOLE_H */
