/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <types.h>
#include <atomic.h>
#include <spinlock.h>
#include <cpu.h>
#include <timer.h>
#include "lib.h"

build_atomic_xadd(atomic_xadd32, "l", int32_t)

static inline int32_t atomic_add_return(int32_t *p, int32_t v)
{
	return (atomic_xadd32(p, v) + v);
}

static inline int32_t atomic_inc_return(int32_t *v)
{
	return atomic_add_return(v, 1);
}

/* Logging flags */
#define LOG_FLAG_STDOUT		0x00000001U
#define LOG_ENTRY_SIZE	80U
/* Size of buffer used to store a message being logged,
 * should align to LOG_ENTRY_SIZE.
 */
#define LOG_MESSAGE_MAX_SIZE	(4U * LOG_ENTRY_SIZE)

extern uint16_t console_loglevel;

static char logbuf[CONFIG_MAX_PCPU_NUM][LOG_MESSAGE_MAX_SIZE];

struct acrn_logmsg_ctl {
	uint32_t flags;
	int32_t seq;
	spinlock_t lock;
};

static struct acrn_logmsg_ctl logmsg_ctl;

void init_logmsg(uint32_t flags)
{
	logmsg_ctl.flags = flags;
	logmsg_ctl.seq = 0;
}

void do_logmsg(uint32_t severity, __unused const char *fmt, ...)
{
	va_list args;
	uint64_t timestamp, rflags;
	uint16_t pcpu_id;
	bool do_console_log;
	char *buffer;

	do_console_log = (((logmsg_ctl.flags & LOG_FLAG_STDOUT) != 0U) && (severity <= console_loglevel));

	if (!do_console_log) {
		return;
	}

	/* Get time-stamp value */
	timestamp = rdtsc();

	/* Scale time-stamp appropriately */
	timestamp = ticks_to_us(timestamp);

	/* Get CPU ID */
	pcpu_id = get_pcpu_id();
	buffer = logbuf[pcpu_id];

	(void)memset(buffer, 0U, LOG_MESSAGE_MAX_SIZE);
	/* Put time-stamp, CPU ID and severity into buffer */
	snprintf(buffer, LOG_MESSAGE_MAX_SIZE, "[%lluus][cpu=%hu][sev=%u][seq=%u]:",
			timestamp, pcpu_id, severity, atomic_inc_return(&logmsg_ctl.seq));

	/* Put message into remaining portion of local buffer */
	va_start(args, fmt);
	vsnprintf(buffer + strnlen_s(buffer, LOG_MESSAGE_MAX_SIZE),
		LOG_MESSAGE_MAX_SIZE
		- strnlen_s(buffer, LOG_MESSAGE_MAX_SIZE), fmt, args);
	va_end(args);

	/* Check if flags specify to output to stdout */
	if (do_console_log) {
		spinlock_irqsave_obtain(&(logmsg_ctl.lock), &rflags);

		/* Send buffer to stdout */
		printf("%s\n", buffer);

		spinlock_irqrestore_release(&(logmsg_ctl.lock), rflags);
	}
}
