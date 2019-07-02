/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <types.h>
#include <errno.h>
#include <guest_memory.h>
#include <vcpu.h>
#include <vm.h>
#include <vmcs.h>
#include <mmu.h>
#include <ept.h>
#include <logmsg.h>

static inline uint32_t local_copy_gpa(struct acrn_vm *vm, void *h_ptr, uint64_t gpa,
	uint32_t size, uint32_t fix_pg_size, bool cp_from_vm)
{
	uint64_t hpa;
	uint32_t offset_in_pg, len, pg_size;
	void *g_ptr;

	hpa = local_gpa2hpa(vm, gpa, &pg_size);
	if (hpa == INVALID_HPA) {
		pr_err("%s,vm[%hu] gpa 0x%llx,GPA is unmapping",
			__func__, vm->vm_id, gpa);
		len = 0U;
	} else {

		if (fix_pg_size != 0U) {
			pg_size = fix_pg_size;
		}

		offset_in_pg = (uint32_t)gpa & (pg_size - 1U);
		len = (size > (pg_size - offset_in_pg)) ? (pg_size - offset_in_pg) : size;

		g_ptr = hpa2hva(hpa);

		stac();
		if (cp_from_vm) {
			(void)memcpy_s(h_ptr, len, g_ptr, len);
		} else {
			(void)memcpy_s(g_ptr, len, h_ptr, len);
		}
		clac();
	}

	return len;
}

static inline int32_t copy_gpa(struct acrn_vm *vm, void *h_ptr_arg, uint64_t gpa_arg,
	uint32_t size_arg, bool cp_from_vm)
{
	void *h_ptr = h_ptr_arg;
	uint32_t len;
	uint64_t gpa = gpa_arg;
	uint32_t size = size_arg;
	int32_t err = 0;

	while (size > 0U) {
		len = local_copy_gpa(vm, h_ptr, gpa, size, 0U, cp_from_vm);
		if (len == 0U) {
			err = -EINVAL;
			break;
		}
		gpa += len;
		h_ptr += len;
		size -= len;
	}

	return err;
}

/* @pre Caller(Guest) should make sure gpa is continuous.
 * - gpa from hypercall input which from kernel stack is gpa continuous, not
 *   support kernel stack from vmap
 * - some other gpa from hypercall parameters, VHM should make sure it's
 *   continuous
 * @pre Pointer vm is non-NULL
 */
int32_t copy_to_gpa(struct acrn_vm *vm, void *h_ptr, uint64_t gpa, uint32_t size)
{
	return copy_gpa(vm, h_ptr, gpa, size, 0);
}

/* gpa --> hpa -->hva */
void *gpa2hva(struct acrn_vm *vm, uint64_t x)
{
	return hpa2hva(gpa2hpa(vm, x));
}
