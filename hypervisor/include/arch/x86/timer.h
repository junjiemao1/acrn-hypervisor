/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef TIMER_H
#define TIMER_H

#include <list.h>

/**
 * @brief Timer
 *
 * @defgroup timer ACRN Timer
 * @{
 */

/* External Interfaces */

#define CYCLES_PER_MS	us_to_ticks(1000U)

void udelay(uint32_t us);

/**
 * @brief convert us to ticks.
 *
 * @return ticks
 */
uint64_t us_to_ticks(uint32_t us);

/**
 * @brief convert ticks to us.
 *
 * @return microsecond
 */
uint64_t ticks_to_us(uint64_t ticks);

/**
 * @brief read tsc.
 *
 * @return tsc value
 */
uint64_t rdtsc(void);

/**
 * @brief Calibrate tsc.
 *
 * @return None
 */
void calibrate_tsc(void);

/**
 * @brief  Get tsc.
 *
 * @return tsc(KHz)
 */
uint32_t get_tsc_khz(void);

/**
 * @}
 */

#endif /* TIMER_H */
