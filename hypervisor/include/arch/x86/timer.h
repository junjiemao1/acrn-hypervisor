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

/**
 * @brief Definition of timer tick mode
 */
enum tick_mode {
	TICK_MODE_ONESHOT = 0,	/**< one-shot mode */
	TICK_MODE_PERIODIC,	/**< periodic mode */
};

/**
 * @brief Definition of timer
 */
struct hv_timer {
	enum tick_mode mode;		/**< timer mode: one-shot or periodic */
	uint64_t fire_tsc;		/**< tsc deadline to interrupt */
	uint64_t period_in_cycle;	/**< period of the periodic timer in unit of TSC cycles */
};

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
