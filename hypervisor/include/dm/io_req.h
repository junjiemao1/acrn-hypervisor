/*
 * Copyright (C) 2019 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef IOREQ_H
#define IOREQ_H

#include <types.h>
#include <acrn_common.h>
#include <list.h>

/**
 * @brief I/O Emulation
 *
 * @defgroup ioemul ACRN I/O Emulation
 * @{
 */

/**
 * @brief Internal representation of a I/O request.
 */
struct io_request {
	/**
	 * @brief Type of the request (PIO, MMIO, etc).
	 *
	 * Refer to vhm_request for detailed description of I/O request types.
	 */
	uint32_t io_type;

	/**
	 * @brief Details of this request in the same format as vhm_request.
	 */
	union vhm_io_request reqs;
};

/**
 * @brief Definition of a IO port range
 */
struct vm_io_range {
	uint16_t base;		/**< IO port base */
	uint16_t len;		/**< IO port range */
	uint32_t flags;		/**< IO port attributes */
};

struct vm_io_handler_desc;
struct acrn_vm;
struct acrn_vcpu;

typedef
bool (*io_read_fn_t)(struct acrn_vm *vm, struct acrn_vcpu *vcpu, uint16_t port, size_t size);

typedef
bool (*io_write_fn_t)(struct acrn_vm *vm, uint16_t port, size_t size, uint32_t val);

/**
 * @brief Describes a single IO handler description entry.
 */
struct vm_io_handler_desc {

	/**
	 * @brief The base port number of the IO range for this description.
	 */
	uint16_t port_start;

	/**
	 * @brief The last port number of the IO range for this description (non-inclusive).
	 */
	uint16_t port_end;

	/**
	 * @brief A pointer to the "read" function.
	 *
	 * The read function is called from the hypervisor whenever
	 * a read access to a range described in "ranges" occur.
	 * The arguments to the callback are:
	 *
	 *    - The address of the port to read from.
	 *    - The width of the read operation (1,2 or 4).
	 *
	 * The implementation must return the ports content as
	 * byte, word or doubleword (depending on the width).
	 *
	 * If the pointer is null, a read of 1's is assumed.
	 */
	io_read_fn_t io_read;

	/**
	 * @brief A pointer to the "write" function.
	 *
	 * The write function is called from the hypervisor code
	 * whenever a write access to a range described in "ranges"
	 * occur. The arguments to the callback are:
	 *
	 *   - The address of the port to write to.
	 *   - The width of the write operation (1,2 or 4).
	 *   - The value to write as byte, word or doubleword
	 *     (depending on the width)
	 *
	 * The implementation must write the value to the port.
	 *
	 * If the pointer is null, the write access is ignored.
	 */
	io_write_fn_t io_write;
};

#define IO_ATTR_RW	      1U

/* Typedef for MMIO handler and range check routine */
struct mmio_request;
typedef int32_t (*hv_mem_io_handler_t)(struct io_request *io_req, void *handler_private_data);

/* External Interfaces */

/**
 * @brief Emulate \p io_req for \p vcpu
 *
 * Handle an I/O request by either invoking a hypervisor-internal handler or
 * deliver to VHM.
 *
 * @pre vcpu != NULL
 * @pre vcpu->vm != NULL
 * @pre vcpu->vm->vm_id < CONFIG_MAX_VM_NUM
 *
 * @param vcpu The virtual CPU that triggers the MMIO access
 * @param io_req The I/O request holding the details of the MMIO access
 *
 * @retval 0 Successfully emulated by registered handlers.
 * @retval IOREQ_PENDING The I/O request is delivered to VHM.
 * @retval -EIO The request spans multiple devices and cannot be emulated.
 * @retval -EINVAL \p io_req has an invalid type.
 * @retval <0 on other errors during emulation.
 */
int32_t emulate_io(struct acrn_vcpu *vcpu, struct io_request *io_req);

/**
 * @brief Register a port I/O handler
 *
 * @param vm      The VM to which the port I/O handlers are registered
 * @param pio_idx The emulated port io index
 * @param range   The emulated port io range
 * @param io_read_fn_ptr The handler for emulating reads from the given range
 * @param io_write_fn_ptr The handler for emulating writes to the given range
 * @pre pio_idx < EMUL_PIO_IDX_MAX
 */
void   register_pio_emulation_handler(struct acrn_vm *vm, uint32_t pio_idx,
		const struct vm_io_range *range, io_read_fn_t io_read_fn_ptr, io_write_fn_t io_write_fn_ptr);

/**
 * @brief Register port I/O default handler
 *
 * @param vm      The VM to which the port I/O handlers are registered
 */
void register_pio_default_emulation_handler(struct acrn_vm *vm);

/**
 * @brief Register MMIO default handler
 *
 * @param vm The VM to which the MMIO handler is registered
 */
void register_mmio_default_emulation_handler(struct acrn_vm *vm);

/**
 * @}
 */

#endif /* IOREQ_H */
