/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <types.h>
#include <errno.h>
#include <sprintf.h>
#include <vm.h>
#include <vm_reset.h>
#include <bits.h>
#include <e820.h>
#include <multiboot.h>
#include <vtd.h>
#include <reloc.h>
#include <ept.h>
#include <guest_pm.h>
#include <console.h>
#include <ptdev.h>
#include <vmcs.h>
#include <pgtable.h>
#include <mmu.h>
#include <logmsg.h>
#include <vboot.h>
#include <vboot_info.h>
#include <board.h>
#include <sgx.h>
#include <sbuf.h>

vm_sw_loader_t vm_sw_loader;

/* Local variables */

static struct acrn_vm vm_array[CONFIG_MAX_VM_NUM] __aligned(PAGE_SIZE);

bool is_sos_vm(const struct acrn_vm *vm)
{
	return (vm != NULL)  && (get_vm_config(vm->vm_id)->load_order == SOS_VM);
}

/**
 * @pre vm != NULL
 * @pre vm->vmid < CONFIG_MAX_VM_NUM
 */
bool is_prelaunched_vm(const struct acrn_vm *vm)
{
	struct acrn_vm_config *vm_config;

	vm_config = get_vm_config(vm->vm_id);
	return (vm_config->load_order == PRE_LAUNCHED_VM);
}

/**
 * @pre vm != NULL && vm_config != NULL && vm->vmid < CONFIG_MAX_VM_NUM
 */
bool is_lapic_pt_configured(const struct acrn_vm *vm)
{
	struct acrn_vm_config *vm_config = get_vm_config(vm->vm_id);

	return ((vm_config->guest_flags & GUEST_FLAG_LAPIC_PASSTHROUGH) != 0U);
}

/**
 * @pre vm != NULL && vm_config != NULL && vm->vmid < CONFIG_MAX_VM_NUM
 */
bool is_rt_vm(const struct acrn_vm *vm)
{
	struct acrn_vm_config *vm_config = get_vm_config(vm->vm_id);

	return ((vm_config->guest_flags & GUEST_FLAG_RT) != 0U);
}

/**
 * @brief Initialize the I/O bitmap for \p vm
 *
 * @param vm The VM whose I/O bitmap is to be initialized
 */
static void setup_io_bitmap(struct acrn_vm *vm)
{
	/* block all IO port access from Guest */
	(void)memset(vm->arch_vm.io_bitmap, 0xFFU, PAGE_SIZE * 2U);
}

/**
 * return a pointer to the virtual machine structure associated with
 * this VM ID
 *
 * @pre vm_id < CONFIG_MAX_VM_NUM
 */
struct acrn_vm *get_vm_from_vmid(uint16_t vm_id)
{
	return &vm_array[vm_id];
}

/**
 * @pre vm_config != NULL
 */
static inline uint16_t get_vm_bsp_pcpu_id(const struct acrn_vm_config *vm_config)
{
	uint16_t cpu_id = INVALID_CPU_ID;

	cpu_id = ffs64(vm_config->pcpu_bitmap);

	return (cpu_id < get_pcpu_nums()) ? cpu_id : INVALID_CPU_ID;
}

/**
 * @pre vm != NULL && vm_config != NULL
 */
static void prepare_prelaunched_vm_memmap(struct acrn_vm *vm, const struct acrn_vm_config *vm_config)
{
	uint64_t base_hpa = vm_config->memory.start_hpa;
	uint32_t i;

	for (i = 0U; i < vm->e820_entry_num; i++) {
		struct e820_entry *entry = &(vm->e820_entries[i]);

		if (entry->length == 0UL) {
			break;
		}

		/* Do EPT mapping for GPAs that are backed by physical memory */
		if (entry->type == E820_TYPE_RAM) {
			ept_add_mr(vm, (uint64_t *)vm->arch_vm.nworld_eptp, base_hpa, entry->baseaddr,
				entry->length, EPT_RWX | EPT_WB);

			base_hpa += entry->length;
		}

		/* GPAs under 1MB are always backed by physical memory */
		if ((entry->type != E820_TYPE_RAM) && (entry->baseaddr < (uint64_t)MEM_1M)) {
			ept_add_mr(vm, (uint64_t *)vm->arch_vm.nworld_eptp, base_hpa, entry->baseaddr,
				entry->length, EPT_RWX | EPT_UNCACHED);

			base_hpa += entry->length;
		}
	}
}

/* Add EPT mapping of EPC reource for the VM */
static void prepare_epc_vm_memmap(struct acrn_vm *vm)
{
	struct epc_map* vm_epc_maps;
	uint32_t i;

}

static void register_pm_io_handler(struct acrn_vm *vm)
{

	/* Intercept the virtual pm port for RTVM */
	if (is_rt_vm(vm)) {
	}
}

/**
 * @pre vm_id < CONFIG_MAX_VM_NUM && vm_config != NULL && rtn_vm != NULL
 * @pre vm->state == VM_POWERED_OFF
 */
int32_t create_vm(uint16_t vm_id, struct acrn_vm_config *vm_config, struct acrn_vm **rtn_vm)
{
	struct acrn_vm *vm = NULL;
	int32_t status = 0;
	bool need_cleanup = false;

	/* Allocate memory for virtual machine */
	vm = &vm_array[vm_id];
	(void)memset((void *)vm, 0U, sizeof(struct acrn_vm));
	vm->vm_id = vm_id;
	vm->hw.created_vcpus = 0U;
	vm->emul_mmio_regions = 0U;

	init_ept_mem_ops(vm);
	vm->arch_vm.nworld_eptp = vm->arch_vm.ept_mem_ops.get_pml4_page(vm->arch_vm.ept_mem_ops.info);
	sanitize_pte((uint64_t *)vm->arch_vm.nworld_eptp);

	register_pio_default_emulation_handler(vm);
	register_mmio_default_emulation_handler(vm);

	(void)memcpy_s(&vm->uuid[0], sizeof(vm->uuid),
		&vm_config->uuid[0], sizeof(vm_config->uuid));

	/* For PRE_LAUNCHED_VM and POST_LAUNCHED_VM */
	if ((vm_config->guest_flags & GUEST_FLAG_SECURE_WORLD_ENABLED) != 0U) {
		vm->sworld_control.flag.supported = 1U;
	}
	if (vm->sworld_control.flag.supported != 0UL) {
		struct memory_ops *ept_mem_ops = &vm->arch_vm.ept_mem_ops;

		ept_add_mr(vm, (uint64_t *)vm->arch_vm.nworld_eptp,
			hva2hpa(ept_mem_ops->get_sworld_memory_base(ept_mem_ops->info)),
			TRUSTY_EPT_REBASE_GPA, TRUSTY_RAM_SIZE, EPT_WB | EPT_RWX);
	}

	create_prelaunched_vm_e820(vm);
	prepare_prelaunched_vm_memmap(vm, vm_config);
	status = init_vm_boot_info(vm);

	if (status == 0) {
		prepare_epc_vm_memmap(vm);

		INIT_LIST_HEAD(&vm->softirq_dev_entry_list);
		spinlock_init(&vm->softirq_dev_lock);
		spinlock_init(&vm->vm_lock);

		vm->arch_vm.vlapic_state = VM_VLAPIC_XAPIC;
		vm->intr_inject_delay_delta = 0UL;

		/* Set up IO bit-mask such that VM exit occurs on
		 * selected IO ranges
		 */
		setup_io_bitmap(vm);

		register_pm_io_handler(vm);

		/* Create virtual uart;*/
		vuart_init(vm, vm_config->vuart);

		vrtc_init(vm);

		vpci_init(vm);
		enable_iommu();

		/* vpic wire_mode default is INTR */
		vm->wire_mode = VPIC_WIRE_INTR;

		/* Populate return VM handle */
		*rtn_vm = vm;
		vm->sw.io_shared_page = NULL;
		status = set_vcpuid_entries(vm);
		if (status == 0) {
			vm->state = VM_CREATED;
		} else {
			need_cleanup = true;
		}
	}

	if (need_cleanup) {
		if (vm->arch_vm.nworld_eptp != NULL) {
			(void)memset(vm->arch_vm.nworld_eptp, 0U, PAGE_SIZE);
		}
	}
	return status;
}

/*
 * @pre vm != NULL
 */
int32_t shutdown_vm(struct acrn_vm *vm)
{
	uint16_t i;
	uint64_t mask = 0UL;
	struct acrn_vcpu *vcpu = NULL;
	struct acrn_vm_config *vm_config = NULL;
	int32_t ret = 0;

	pause_vm(vm);

	/* Only allow shutdown paused vm */
	if (vm->state == VM_PAUSED) {
		vm->state = VM_POWERED_OFF;

		foreach_vcpu(i, vm, vcpu) {
			reset_vcpu(vcpu);
			offline_vcpu(vcpu);

			bitmap_set_nolock(vcpu->pcpu_id, &mask);
			make_pcpu_offline(vcpu->pcpu_id);
		}

		wait_pcpus_offline(mask);

		if (is_lapic_pt_configured(vm) && !start_pcpus(mask)) {
			pr_fatal("Failed to start all cpus in mask(0x%llx)", mask);
			ret = -ETIMEDOUT;
		}

		vm_config = get_vm_config(vm->vm_id);
		vm_config->guest_flags &= ~DM_OWNED_GUEST_FLAG_MASK;

		vpci_cleanup(vm);

		vuart_deinit(vm);

		ptdev_release_all_entries(vm);

		/* Free iommu */
		if (vm->iommu != NULL) {
			destroy_iommu_domain(vm->iommu);
		}

		/* Free EPT allocated resources assigned to VM */
		destroy_ept(vm);

		ret = 0;
	} else {
		ret = -EINVAL;
	}

	/* Return status to caller */
	return ret;
}

/**
 *  * @pre vm != NULL
 */
void start_vm(struct acrn_vm *vm)
{
	struct acrn_vcpu *vcpu = NULL;

	vm->state = VM_STARTED;

	/* Only start BSP (vid = 0) and let BSP start other APs */
	vcpu = vcpu_from_vid(vm, 0U);
	schedule_vcpu(vcpu);
}

/**
 *  * @pre vm != NULL
 */
void pause_vm(struct acrn_vm *vm)
{
	uint16_t i;
	struct acrn_vcpu *vcpu = NULL;

	if (vm->state != VM_PAUSED) {
		if (is_rt_vm(vm)) {
			/**
			 * For RTVM, we can only pause its vCPUs when it stays at following states:
			 *  - It is powering off by itself
			 *  - It is created but doesn't start
			 */
			if ((vm->state == VM_POWERING_OFF) || (vm->state == VM_CREATED)) {
				foreach_vcpu(i, vm, vcpu) {
					pause_vcpu(vcpu, VCPU_ZOMBIE);
				}

				vm->state = VM_PAUSED;
			}
		} else {
			foreach_vcpu(i, vm, vcpu) {
				pause_vcpu(vcpu, VCPU_ZOMBIE);
			}

			vm->state = VM_PAUSED;
		}
	}
}

/**
 * Prepare to create vm/vcpu for vm
 *
 * @pre vm_id < CONFIG_MAX_VM_NUM && vm_config != NULL
 */
void prepare_vm(uint16_t vm_id, struct acrn_vm_config *vm_config)
{
	int32_t err = 0;
	uint16_t i;
	struct acrn_vm *vm = NULL;

	err = create_vm(vm_id, vm_config, &vm);

	if (err == 0) {
		for (i = 0U; i < get_pcpu_nums(); i++) {
			if (bitmap_test(i, &vm_config->pcpu_bitmap)) {
				err = prepare_vcpu(vm, i);
				if (err != 0) {
					break;
				}
			}
		}
	}

	if (err == 0) {
		(void)mptable_build(vm);

		(void )vm_sw_loader(vm);

		/* start vm BSP automatically */
		start_vm(vm);

		pr_acrnlog("Start VM id: %x name: %s", vm_id, vm_config->name);
	}
}

/**
 * @pre vm_config != NULL
 */
void launch_vms(uint16_t pcpu_id)
{
	uint16_t vm_id, bsp_id;
	struct acrn_vm_config *vm_config;

	for (vm_id = 0U; vm_id < CONFIG_MAX_VM_NUM; vm_id++) {
		vm_config = get_vm_config(vm_id);

		bsp_id = get_vm_bsp_pcpu_id(vm_config);
		if (pcpu_id == bsp_id) {
			prepare_vm(vm_id, vm_config);
		}
	}
}

/*
 * @brief Update state of vLAPICs of a VM
 * vLAPICs of VM switch between modes in an asynchronous fashion. This API
 * captures the "transition" state triggered when one vLAPIC switches mode.
 * When the VM is created, the state is set to "xAPIC" as all vLAPICs are setup
 * in xAPIC mode.
 *
 * Upon reset, all LAPICs switch to xAPIC mode accroding to SDM 10.12.5
 * Considering VM uses x2apic mode for vLAPIC, in reset or shutdown flow, vLAPIC state
 * moves to "xAPIC" directly without going thru "transition".
 *
 * VM_VLAPIC_X2APIC - All the online vCPUs/vLAPICs of this VM use x2APIC mode
 * VM_VLAPIC_XAPIC - All the online vCPUs/vLAPICs of this VM use xAPIC mode
 * VM_VLAPIC_DISABLED - All the online vCPUs/vLAPICs of this VM are in Disabled mode
 * VM_VLAPIC_TRANSITION - Online vCPUs/vLAPICs of this VM are in between transistion
 *
 * TODO: offline_vcpu need to call this API to reflect the status of rest of the
 * vLAPICs that are online.
 *
 * @pre vm != NULL
 */
void update_vm_vlapic_state(struct acrn_vm *vm)
{
	uint16_t i;
	struct acrn_vcpu *vcpu;
	uint16_t vcpus_in_x2apic, vcpus_in_xapic;
	enum vm_vlapic_state vlapic_state = VM_VLAPIC_XAPIC;

	vcpus_in_x2apic = 0U;
	vcpus_in_xapic = 0U;
	spinlock_obtain(&vm->vm_lock);
	foreach_vcpu(i, vm, vcpu) {
		vcpus_in_x2apic++;
	}

	if ((vcpus_in_x2apic == 0U) && (vcpus_in_xapic == 0U)) {
		/*
		 * Check if the counts vcpus_in_x2apic and vcpus_in_xapic are zero
		 * VM_VLAPIC_DISABLED
		 */
		vlapic_state = VM_VLAPIC_DISABLED;
	} else if ((vcpus_in_x2apic != 0U) && (vcpus_in_xapic != 0U)) {
		/*
		 * Check if the counts vcpus_in_x2apic and vcpus_in_xapic are non-zero
		 * VM_VLAPIC_TRANSITION
		 */
		vlapic_state = VM_VLAPIC_TRANSITION;
	} else if (vcpus_in_x2apic != 0U) {
		/*
		 * Check if the counts vcpus_in_x2apic is non-zero
		 * VM_VLAPIC_X2APIC
		 */
		vlapic_state = VM_VLAPIC_X2APIC;
	} else {
		/*
		 * Count vcpus_in_xapic is non-zero
		 * VM_VLAPIC_XAPIC
		 */
		vlapic_state = VM_VLAPIC_XAPIC;
	}

	vm->arch_vm.vlapic_state = vlapic_state;
	spinlock_release(&vm->vm_lock);
}

/*
 * @brief Check state of vLAPICs of a VM
 *
 * @pre vm != NULL
 */
enum vm_vlapic_state check_vm_vlapic_state(const struct acrn_vm *vm)
{
	enum vm_vlapic_state vlapic_state;

	vlapic_state = vm->arch_vm.vlapic_state;
	return vlapic_state;
}

/**
 * if there is RT VM return true otherwise return false.
 */
bool has_rt_vm(void)
{
	uint16_t vm_id;

	for (vm_id = 0U; vm_id < CONFIG_MAX_VM_NUM; vm_id++) {
		if (is_rt_vm(get_vm_from_vmid(vm_id))) {
			break;
		}
	}

	return ((vm_id == CONFIG_MAX_VM_NUM) ? false : true);
}
