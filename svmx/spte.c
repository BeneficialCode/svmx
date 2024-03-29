#include "pch.h"
#include "mmu.h"
#include "spte.h"
#include "vmx.h"

u64 shadow_host_writable_mask;
u64 shadow_mmu_writable_mask;
u64 shadow_nx_mask;
u64 shadow_x_mask; /* mutual exclusive with nx_mask */
u64 shadow_user_mask;
u64 shadow_accessed_mask;
u64 shadow_dirty_mask;
u64 shadow_mmio_value;
u64 shadow_mmio_mask;
u64 shadow_mmio_access_mask;
u64 shadow_present_mask;
u64 shadow_memtype_mask;
u64 shadow_me_value;
u64 shadow_me_mask;
u64 shadow_acc_track_mask;

u64 shadow_nonpresent_or_rsvd_mask;
u64 shadow_nonpresent_or_rsvd_lower_gfn_mask;

u8 shadow_phys_bits;

bool enable_mmio_caching = TRUE;
static bool allow_mmio_caching;

void kvm_mmu_set_ept_masks(bool has_ad_bits, bool has_exec_only) {
	shadow_user_mask = VMX_EPT_READABLE_MASK;
	shadow_accessed_mask = has_ad_bits ? VMX_EPT_ACCESS_BIT : 0ull;
	shadow_dirty_mask = has_ad_bits ? VMX_EPT_DIRTY_BIT : 0ull;
	shadow_nx_mask = 0ull;
	shadow_x_mask = VMX_EPT_EXECUTABLE_MASK;
	shadow_present_mask = has_exec_only ? 0ull : VMX_EPT_READABLE_MASK;

	/*
	* EPT overrides the host MTRRs, and so KVM must program the desired
	* memtype directly into the SPTEs. Note, this mask is just the mask
	* of all bits that factor into the memtype, the actual memtype must be
	* dynamically calculated, e.g. to ensure host MMIO is mapped UC.
	*/
	shadow_memtype_mask = VMX_EPT_MT_MASK | VMX_EPT_IPAT_BIT;
	shadow_acc_track_mask = VMX_EPT_RWX_MASK;
	shadow_host_writable_mask = EPT_SPTE_HOST_WRITABLE;
	shadow_mmu_writable_mask = EPT_SPTE_MMU_WRITABLE;

	/*
	* EPT Misconfigurations are generated if the value of bits 2:0
	* of an EPT paging-structure entry is 110b (write/execute).
	*/
	kvm_mmu_set_mmio_spte_mask(VMX_EPT_MISCONFIG_WX_VALUE,
		VMX_EPT_RWX_MASK, 0);
}

void kvm_mmu_set_mmio_spte_mask(u64 mmio_value, u64 mmio_mask, u64 access_mask) {
	/*
	* Reset to the original module param value to honor userspace's desire
	* to disallow MMIO caching. Update the param itself so that
	* userspace can see whether or not KVM is actually using MMIO caching.
	*/
	enable_mmio_caching = allow_mmio_caching;
	if (!enable_mmio_caching)
		mmio_value = 0;

	/*
	* The mask must contain only bits that are carved out specifically for
	* the MMIO SPTE mask, e.g. to ensure there's no overlap with the MMIO
	* generation.
	*/
	if (mmio_mask & ~SPTE_MMIO_ALLOWED_MASK)
		mmio_value = 0;

	shadow_mmio_value = mmio_value;
	shadow_mmio_mask = mmio_mask;
	shadow_mmio_access_mask = access_mask;
}

void kvm_mmu_reset_all_pte_masks(void) {
	// u8 low_phys_bits;
	u64 mask;

	shadow_phys_bits = kvm_get_shadow_phys_bits();

	/*
	* If the CPU has 46 or less physical address bits, then set an
	* appropriate mask to gurad against L1TF attacks. Otherwise, it it
	* assumed that the CPU is not vulnerable to L1TF.
	* 
	* Some Intel CPUs address the L1 cache using more PA bits than are
	* reported by cpuid. Use the PA width of the L1 cache when possible
	* to achieve more effective mitigation, e.g. if system RAM overlaps
	* the most significant bits of legal physical address space.
	* 
	*/
	shadow_nonpresent_or_rsvd_mask = 0;
	
	shadow_user_mask = PT_USER_MASK;
	shadow_accessed_mask = PT_ACCESSED_MASK;
	shadow_dirty_mask = PT_DIRTY_MASK;
	shadow_nx_mask = PT64_NX_MASK;
	shadow_x_mask = 0;
	shadow_present_mask = PT_PRESENT_MASK;

	/*
	* For shadow paging and NTP, KVM uses PAT entry '0' to encode WB
	* memtype in the SPTEs, i.e. relies on host MTRRs to provide the
	* correct memtype (WB is the "weakest" memtype).
	*/
	shadow_memtype_mask = 0;
	shadow_acc_track_mask = 0;
	shadow_me_mask = 0;
	shadow_me_value = 0;

	shadow_host_writable_mask = DEFAULT_SPTE_HOST_WRITABLE;
	shadow_mmu_writable_mask = DEFAULT_SPTE_MMU_WRITABLE;

	/*
	* Set a reserved PA bit in MMIO SPTEs to generate page faults with
	* PFEC.RSVD=1 on MMIO accesses. 64-bit PTEs (PAE,x86-64, and EPT
	* paging) support a maximum of 52 bits of PA, i.e. if the cpu supports
	* 52-bit physical addresses then there are no reserved PA bits in the 
	* PTEs and so the reserved PA approach must be disabled.
	*/
	if (shadow_phys_bits < 52)
		mask = BIT_ULL(51) | PT_PRESENT_MASK;
	else
		mask = 0;

	kvm_mmu_set_mmio_spte_mask(mask, mask, ACC_WRITE_MASK | ACC_USER_MASK);
}

void kvm_mmu_spte_module_init(void) {
	/*
	 * Snapshot userspace's desire to allow MMIO caching.  Whether or not
	 * KVM can actually enable MMIO caching depends on vendor-specific
	 * hardware capabilities and other module params that can't be resolved
	 * until the vendor module is loaded, i.e. enable_mmio_caching can and
	 * will change when the vendor module is (re)loaded.
	 */
	allow_mmio_caching = enable_mmio_caching;
}

u64 make_nonleaf_spte(u64* child_pt, bool ad_disabled) {
	u64 spte = SPTE_MMU_PRESENT_MASK;
	
	PHYSICAL_ADDRESS physic = MmGetPhysicalAddress(child_pt);
	spte |= physic.QuadPart | shadow_present_mask | PT_WRITABLE_MASK |
		shadow_user_mask | shadow_x_mask | shadow_me_value;

	if (ad_disabled)
		spte |= SPTE_TDP_AD_DISABLED;
	else
		spte |= shadow_accessed_mask;


	return spte;
}


static u64 generation_mmio_spte_mask(u64 gen) {
	u64 mask;

	mask = (gen << MMIO_SPTE_GEN_LOW_SHIFT) & MMIO_SPTE_GEN_LOW_MASK;
	mask |= (gen << MMIO_SPTE_GEN_HIGH_SHIFT) & MMIO_SPTE_GEN_HIGH_MASK;
	return mask;
}

u64 make_mmio_spte(struct kvm_vcpu* vcpu, u64 gfn, unsigned int access) {
	u64 gen = kvm_vcpu_memslots(vcpu)->generation & MMIO_SPTE_GEN_MASK;
	u64 spte = generation_mmio_spte_mask(gen);
	u64 gpa = gfn << PAGE_SHIFT;

	access &= shadow_mmio_access_mask;
	spte |= shadow_mmio_value | access;
	spte |= gpa | shadow_nonpresent_or_rsvd_mask;
	spte |= (gpa & shadow_nonpresent_or_rsvd_mask)
		<< SHADOW_NONPRESENT_OR_RSVD_MASK_LEN;

	return spte;
}

static bool kvm_is_mmio_pfn(kvm_pfn_t pfn) {
	UNREFERENCED_PARAMETER(pfn);
	return FALSE;
}

bool make_spte(struct kvm_vcpu* vcpu, struct kvm_mmu_page* sp,
	const struct kvm_memory_slot* slot,
	unsigned int pte_access, gfn_t gfn, kvm_pfn_t pfn,
	u64 old_spte, bool prefetch, bool can_unsync,
	bool host_writable, u64* new_spte) {
	UNREFERENCED_PARAMETER(can_unsync);
	UNREFERENCED_PARAMETER(old_spte);
	UNREFERENCED_PARAMETER(gfn);
	UNREFERENCED_PARAMETER(slot);

	int level = sp->role.level;
	u64 spte = SPTE_MMU_PRESENT_MASK;
	bool wrprot = FALSE;

	if (sp->role.ad_disabled)
		spte |= SPTE_TDP_AD_DISABLED;
	else if (kvm_mmu_page_ad_need_write_protect(sp))
		spte |= SPTE_TDP_AD_WRPROT_ONLY;

	/*
	* For the EPT case, shadow_present_mask is 0 if hardware
	* supports exec-only page table entries. In that case,
	* ACC_USER_MASK and shadow_user_mask are used to represent
	* read access. See FNAME(gpte_access) in paging_tmpl.h
	*/
	spte |= shadow_present_mask;
	if (!prefetch)
		spte |= spte_shadow_accessed_mask(spte);

	if (level > PG_LEVEL_4K && (pte_access & ACC_EXEC_MASK) &&
		is_nx_huge_page_enabled(vcpu->kvm)) {
		pte_access &= ~ACC_EXEC_MASK;
	}

	if (pte_access & ACC_EXEC_MASK)
		spte |= shadow_x_mask;
	else
		spte |= shadow_nx_mask;

	if (pte_access & ACC_USER_MASK)
		spte |= shadow_user_mask;

	if (level > PG_LEVEL_4K)
		spte |= PT_PAGE_SIZE_MASK;

	
	if (host_writable)
		spte |= shadow_host_writable_mask;
	else
		pte_access &= ~ACC_WRITE_MASK;

	if (shadow_me_value && !kvm_is_mmio_pfn(pfn))
		spte |= shadow_me_value;

	*new_spte = spte;
	return wrprot;
}