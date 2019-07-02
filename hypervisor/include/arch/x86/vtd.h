/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef VTD_H
#define VTD_H
#include <ptdev.h>
/*
 * Intel IOMMU register specification per version 1.0 public spec.
 */

#define DMAR_CAP_REG    0x8U /* Hardware supported capabilities */
#define DMAR_ECAP_REG   0x10U    /* Extended capabilities supported */
#define DMAR_GCMD_REG   0x18U    /* Global command register */
#define DMAR_GSTS_REG   0x1cU    /* Global status register */
#define DMAR_RTADDR_REG 0x20U    /* Root entry table */
#define DMAR_FSTS_REG   0x34U    /* Fault Status register */
#define DMAR_FECTL_REG  0x38U    /* Fault control register */
#define DMAR_FEDATA_REG 0x3cU    /* Fault event interrupt data register */
#define DMAR_FEADDR_REG 0x40U    /* Fault event interrupt addr register */
#define DMAR_IQT_REG    0x88U    /* Invalidation queue tail register */
#define DMAR_IQA_REG    0x90U    /* Invalidation queue addr register */
#define DMAR_IRTA_REG   0xb8U    /* Interrupt remapping table addr register */

/* Values for entry_type in ACPI_DMAR_DEVICE_SCOPE - device types */
enum acpi_dmar_scope_type {
	ACPI_DMAR_SCOPE_TYPE_NOT_USED       = 0,
	ACPI_DMAR_SCOPE_TYPE_ENDPOINT       = 1,
	ACPI_DMAR_SCOPE_TYPE_BRIDGE	 = 2,
	ACPI_DMAR_SCOPE_TYPE_IOAPIC	 = 3,
	ACPI_DMAR_SCOPE_TYPE_HPET	   = 4,
	ACPI_DMAR_SCOPE_TYPE_NAMESPACE      = 5,
	ACPI_DMAR_SCOPE_TYPE_RESERVED       = 6 /* 6 and greater are reserved */
};

struct iommu_domain {
	bool is_host;
	bool is_tt_ept;     /* if reuse EPT of the domain */
	uint16_t vm_id;
	uint32_t addr_width;   /* address width of the domain */
	uint64_t trans_table_ptr;
	bool iommu_snoop;
};

static inline uint16_t iommu_cap_num_fault_regs(uint64_t cap)
{
	return (((uint16_t)(cap >> 40U) & 0xffU) + 1U);
}

static inline uint8_t iommu_cap_super_page_val(uint64_t cap)
{
	return ((uint8_t)(cap >> 34U) & 0xfU);
}

static inline uint16_t iommu_cap_fault_reg_offset(uint64_t cap)
{
	return (((uint16_t)(cap >> 24U) & 0x3ffU) * 16U);
}

static inline uint8_t iommu_cap_sagaw(uint64_t cap)
{
	return ((uint8_t)(cap >> 8U) & 0x1fU);
}

/*
 * Decoding Extended Capability Register
 */
static inline uint8_t iommu_ecap_c(uint64_t ecap)
{
	return ((uint8_t)(ecap >> 0U) & 1U);
}

static inline uint8_t iommu_ecap_qi(uint64_t ecap)
{
	return ((uint8_t)(ecap >> 1U) & 1U);
}

static inline uint8_t iommu_ecap_ir(uint64_t ecap)
{
	return ((uint8_t)(ecap >> 3U) & 1U);
}

static inline uint8_t iommu_ecap_eim(uint64_t ecap)
{
	return ((uint8_t)(ecap >> 4U) & 1U);
}

static inline uint8_t iommu_ecap_pt(uint64_t ecap)
{
	return ((uint8_t)(ecap >> 6U) & 1U);
}

static inline uint8_t iommu_ecap_sc(uint64_t ecap)
{
	return ((uint8_t)(ecap >> 7U) & 1U);
}

static inline uint16_t iommu_ecap_iro(uint64_t ecap)
{
	return ((uint16_t)(ecap >> 8U) & 0x3ffU);
}

/* GCMD_REG */
#define DMA_GCMD_TE (1U << 31U)
#define DMA_GCMD_SRTP (1U << 30U)
#define DMA_GCMD_QIE (1U << 26U)
#define DMA_GCMD_SIRTP (1U << 24U)
#define DMA_GCMD_IRE (1U << 25U)

/* GSTS_REG */
#define DMA_GSTS_TES (1U << 31U)
#define DMA_GSTS_RTPS (1U << 30U)
#define DMA_GSTS_QIES (1U << 26U)
#define DMA_GSTS_IRTPS (1U << 24U)
#define DMA_GSTS_IRES (1U << 25U)

/* CCMD_REG */
#define DMA_CONTEXT_GLOBAL_INVL (1UL << 4U)
#define DMA_CONTEXT_DOMAIN_INVL (2UL << 4U)
#define DMA_CONTEXT_DEVICE_INVL (3UL << 4U)
static inline uint64_t dma_ccmd_fm(uint8_t fm)
{
	return (((uint64_t)fm & 0x3UL) << 48UL);
}

static inline uint64_t dma_ccmd_sid(uint16_t sid)
{
	return (((uint64_t)sid & 0xffffUL) << 32UL);
}

static inline uint64_t dma_ccmd_did(uint16_t did)
{
	return (((uint64_t)did & 0xffffUL) << 16UL);
}

#define DMA_IOTLB_GLOBAL_INVL			(((uint64_t)1UL) << 4U)
#define DMA_IOTLB_DOMAIN_INVL			(((uint64_t)2UL) << 4U)
#define DMA_IOTLB_PAGE_INVL			(((uint64_t)3UL) << 4U)
#define DMA_IOTLB_DR				(((uint64_t)1UL) << 7U)
#define DMA_IOTLB_DW				(((uint64_t)1UL) << 6U)
static inline uint64_t dma_iotlb_did(uint16_t did)
{
	return (((uint64_t)did & 0xffffUL) << 16UL);
}

/* INVALIDATE_ADDRESS_REG */
static inline uint8_t dma_iotlb_invl_addr_am(uint8_t am)
{
	return (am & 0x3fU);
}

/* IEC_REG */
#define DMAR_IECI_INDEXED		(((uint64_t)1UL) << 4U)
#define DMAR_IEC_GLOBAL_INVL		(((uint64_t)0UL) << 4U)
static inline uint64_t dma_iec_index(uint16_t index, uint8_t index_mask)
{
	return ((((uint64_t)index & 0xFFFFU) << 32U) | (((uint64_t)index_mask & 0x1FU) << 27U));
}

#define DMA_IOTLB_INVL_ADDR_IH_UNMODIFIED	(((uint64_t)1UL) << 6U)

static inline bool dma_fsts_ppf(uint32_t ppf)
{
	return (((ppf >> 1U) & 1U) == 1U);
}

static inline uint8_t dma_fsts_fri(uint32_t fri)
{
	return ((uint8_t)(fri >> 8U) & 0xFFU);
}

/* FRCD_REGs: upper 64 bits*/
static inline bool dma_frcd_up_f(uint64_t up_f)
{
	return (((up_f >> 63U) & 1UL) == 1UL);
}

static inline uint8_t dma_frcd_up_t(uint64_t up_t)
{
	return ((uint8_t)(up_t >> 62U) & 1U);
}

static inline uint8_t dma_frcd_up_fr(uint64_t up_fr)
{
	return ((uint8_t)(up_fr >> 32U) & 0xffU);
}

static inline uint16_t dma_frcd_up_sid(uint64_t up_sid)
{
	return ((uint16_t)up_sid & 0xffffU);
}

#define MAX_DRHDS		4
#define MAX_DRHD_DEVSCOPES	4

#define DRHD_FLAG_INCLUDE_PCI_ALL_MASK      (1U)

struct dmar_dev_scope {
	enum acpi_dmar_scope_type type;
	uint8_t id;
	uint8_t bus;
	uint8_t devfun;
};

struct dmar_drhd {
	uint32_t dev_cnt;
	uint16_t segment;
	uint8_t flags;
	bool ignore;
	uint64_t reg_base_addr;
	/* assume no pci device hotplug support */
	struct dmar_dev_scope *devices;
};

struct dmar_info {
	uint32_t drhd_count;
	struct dmar_drhd *drhd_units;
};

struct dmar_entry {
	uint64_t lo_64;
	uint64_t hi_64;
};

union dmar_ir_entry {
	struct dmar_entry entry;
	struct {
		uint64_t present:1;
		uint64_t fpd:1;
		uint64_t dest_mode:1;
		uint64_t rh:1;
		uint64_t trigger_mode:1;
		uint64_t delivery_mode:3;
		uint64_t sw_bits:4;
		uint64_t rsvd_1:3;
		uint64_t mode:1;
		uint64_t vector:8;
		uint64_t rsvd_2:8;
		uint64_t dest:32;
		uint64_t sid:16;
		uint64_t sq:2;
		uint64_t svt:2;
		uint64_t rsvd_3:44;
	} bits __packed;
};

extern struct dmar_info *get_dmar_info(void);

#ifdef CONFIG_ACPI_PARSE_ENABLED
int32_t parse_dmar_table(struct dmar_info *plat_dmar_info);
#endif

/**
 * @file vtd.h
 *
 * @brief public APIs for VT-d
 */

/**
 * @brief VT-d
 *
 * @defgroup acrn_vtd ACRN VT-d
 * @{
 */

/**
 * @brief iommu domain.
 *
 * This struct declaration for iommu domain.
 *
 */
struct iommu_domain;

/**
 * @brief Assign a device specified by bus & devfun to a iommu domain.
 *
 * Remove the device from the from_domain (if non-NULL), and add it to the to_domain (if non-NULL).
 * API silently fails to add/remove devices to/from domains that are under "Ignored" DMAR units. 
 *
 * @param[in]    from_domain iommu domain from which the device is removed from
 * @param[in]    to_domain iommu domain to which the device is assgined to
 * @param[in]    bus the 8-bit bus number of the device
 * @param[in]    devfun the 8-bit device(5-bit):function(3-bit) of the device
 *
 * @retval 0 on success.
 * @retval 1 fail to unassign the device
 *
 * @pre domain != NULL
 *
 */
int32_t move_pt_device(const struct iommu_domain *from_domain, struct iommu_domain *to_domain, uint8_t bus, uint8_t devfun);

/**
 * @brief Create a iommu domain for a VM specified by vm_id.
 *
 * Create a iommu domain for a VM specified by vm_id, along with address translation table and address width.
 *
 * @param[in] vm_id vm_id of the VM the domain created for
 * @param[in] translation_table the physcial address of EPT table of the VM specified by the vm_id
 * @param[in] addr_width address width of the VM
 *
 * @return Pointer to the created iommu_domain
 *
 * @retval NULL when \p translation_table is 0
 * @retval !NULL when \p translation_table is not 0
 *
 * @pre vm_id is valid
 * @pre translation_table != 0
 *
 */
struct iommu_domain *create_iommu_domain(uint16_t vm_id, uint64_t translation_table, uint32_t addr_width);

/**
 * @brief Destroy the specific iommu domain.
 *
 * Destroy the specific iommu domain when a VM no longer needs it.
 *
 * @param[in] domain iommu domain to destroy
 *
 * @pre domain != NULL
 *
 */
void destroy_iommu_domain(struct iommu_domain *domain);

/**
 * @brief Enable translation of IOMMUs.
 *
 * Enable address translation of all IOMMUs, which are not ignored on the platform.
 *
 */
void enable_iommu(void);

/**
 * @brief Init IOMMUs.
 *
 * Register DMAR units on the platform according to the pre-parsed information
 * or DMAR table. IOMMU is a must have feature, if init_iommu failed, the system
 * should not continue booting.
 *
 * @retval 0 on success
 * @retval <0 on failure
 *
 */
int32_t init_iommu(void);

/**
 * @brief check the iommu if support cache snoop.
 *
 * @param[in] iommu pointer to iommu domain to check
 *
 * @retval true support
 * @retval false not support
 *
 */
bool iommu_snoop_supported(const struct iommu_domain *iommu);

/**
 * @brief Assign RTE for Interrupt Remapping Table.
 *
 * @param[in] intr_src filled with type of interrupt source and the source
 * @param[in] irte filled with info about interrupt deliverymode, destination and destination mode
 * @param[in] index into Interrupt Remapping Table
 *
 * @retval -EINVAL if corresponding DMAR is not present
 * @retval 0 otherwise
 *
 */
int32_t dmar_assign_irte(struct intr_source intr_src, union dmar_ir_entry irte, uint16_t index);

/**
 * @brief Free RTE for Interrupt Remapping Table.
 *
 * @param[in] intr_src filled with type of interrupt source and the source
 * @param[in] index into Interrupt Remapping Table
 *
 */
void dmar_free_irte(struct intr_source intr_src, uint16_t index);
/**
  * @}
  */

#endif
