/*
 * Copyright (C) 2019 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <types.h>
#include <errno.h>
#include <atomic.h>
#include <io_req.h>
#include <vcpu.h>
#include <vm.h>
#include <instr_emul.h>
#include <vmexit.h>
#include <vmx.h>
#include <ept.h>
#include <trace.h>
#include <logmsg.h>

/**
 * @brief General complete-work for port I/O emulation
 *
 * @pre io_req->io_type == REQ_PORTIO
 *
 * @remark This function must be called when \p io_req is completed, after
 * either a previous call to emulate_io() returning 0 or the corresponding VHM
 * request having transferred to the COMPLETE state.
 */
void
emulate_pio_complete(struct acrn_vcpu *vcpu, const struct io_request *io_req)
{
	const struct pio_request *pio_req = &io_req->reqs.pio;
	uint64_t mask = 0xFFFFFFFFUL >> (32UL - (8UL * pio_req->size));

	if (pio_req->direction == REQUEST_READ) {
		uint64_t value = (uint64_t)pio_req->value;
		uint64_t rax = vcpu_get_gpreg(vcpu, CPU_REG_RAX);

		rax = ((rax) & ~mask) | (value & mask);
		vcpu_set_gpreg(vcpu, CPU_REG_RAX, rax);
	}
}

/**
 * @brief The handler of VM exits on I/O instructions
 *
 * @param vcpu The virtual CPU which triggers the VM exit on I/O instruction
 */
int32_t pio_instr_vmexit_handler(struct acrn_vcpu *vcpu)
{
	int32_t status;
	uint64_t exit_qual;
	uint32_t mask;
	int32_t cur_context_idx = vcpu->arch.cur_context;
	struct io_request *io_req = &vcpu->req;
	struct pio_request *pio_req = &io_req->reqs.pio;

	exit_qual = vcpu->arch.exit_qualification;

	io_req->io_type = REQ_PORTIO;
	pio_req->size = vm_exit_io_instruction_size(exit_qual) + 1UL;
	pio_req->address = vm_exit_io_instruction_port_number(exit_qual);
	if (vm_exit_io_instruction_access_direction(exit_qual) == 0UL) {
		mask = 0xFFFFFFFFU >> (32U - (8U * pio_req->size));
		pio_req->direction = REQUEST_WRITE;
		pio_req->value = (uint32_t)vcpu_get_gpreg(vcpu, CPU_REG_RAX) & mask;
	} else {
		pio_req->direction = REQUEST_READ;
	}

	TRACE_4I(TRACE_VMEXIT_IO_INSTRUCTION,
		(uint32_t)pio_req->address,
		(uint32_t)pio_req->direction,
		(uint32_t)pio_req->size,
		(uint32_t)cur_context_idx);

	status = emulate_io(vcpu, io_req);

	return status;
}
