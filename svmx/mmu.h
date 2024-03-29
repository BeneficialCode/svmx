#pragma once
#include "kvm_cache_regs.h"
#include "cpuid.h"

#define PT_WRITABLE_SHIFT 1

#define PT_PRESENT_MASK (1ULL << 0)
#define PT_WRITABLE_MASK (1ULL << PT_WRITABLE_SHIFT)
#define PT_USER_MASK (1ULL << 2)
#define PT_PWT_MASK (1ULL << 3)
#define PT_PCD_MASK (1ULL << 4)
#define PT_ACCESSED_SHIFT 5
#define PT_DIRTY_SHIFT 6
#define PT_ACCESSED_MASK (1ULL << PT_ACCESSED_SHIFT)
#define PT_DIRTY_MASK (1ULL << 6)
#define PT_PAGE_SIZE_MASK (1ULL << 7)
#define PT_PAT_MASK (1ULL << 7)
#define PT_GLOBAL_MASK (1ULL << 8)
#define PT64_NX_SHIFT 63
#define PT64_NX_MASK (1ULL << PT64_NX_SHIFT)

#define PT64_ROOT_5LEVEL 5
#define PT64_ROOT_4LEVEL 4
#define PT32_ROOT_LEVEL 2
#define PT32E_ROOT_LEVEL 3


#define KVM_MMU_CR4_ROLE_BITS (X86_CR4_PSE | X86_CR4_PAE | X86_CR4_LA57 | \
			       X86_CR4_SMEP | X86_CR4_SMAP | X86_CR4_PKE)

#define KVM_MMU_CR0_ROLE_BITS (X86_CR0_PG | X86_CR0_WP)

#define PAGE_MASK		(~(PAGE_SIZE-1))



void kvm_init_mmu(struct kvm_vcpu* vcpu);

void kvm_mmu_unload(struct kvm_vcpu* vcpu);
int kvm_mmu_load(struct kvm_vcpu* vcpu);

static inline int kvm_mmu_reload(struct kvm_vcpu* vcpu)
{
	return kvm_mmu_load(vcpu);
}

gpa_t translate_nested_gpa(struct kvm_vcpu* vcpu, gpa_t gpa, u64 access,
	struct x86_exception* exception);

static inline gpa_t kvm_translate_gpa(struct kvm_vcpu* vcpu,
	struct kvm_mmu* mmu,
	gpa_t gpa, u64 access,
	struct x86_exception* exception)
{
	if (mmu != &vcpu->arch.nested_mmu)
		return gpa;
	return translate_nested_gpa(vcpu, gpa, access, exception);
}

int kvm_tdp_page_fault(struct kvm_vcpu* vcpu, struct kvm_page_fault* fault);

static inline void kvm_mmu_load_pgd(struct kvm_vcpu* vcpu) {
	u64 root_hpa = vcpu->arch.mmu->root.hpa;

	if (!VALID_PAGE(root_hpa))
		return;


	kvm_x86_ops.load_mmu_pgd(vcpu, root_hpa,
		vcpu->arch.mmu->root_role.level);
}

static inline unsigned long kvm_get_pcid(struct kvm_vcpu* vcpu, gpa_t cr3)
{
	return kvm_is_cr4_bit_set(vcpu, X86_CR4_PCIDE)
		? cr3 & X86_CR3_PCID_MASK
		: 0;
}

static inline unsigned long kvm_get_active_pcid(struct kvm_vcpu* vcpu)
{
	return kvm_get_pcid(vcpu, kvm_read_cr3(vcpu));
}

int kvm_mmu_post_init_vm(struct kvm* kvm);
void kvm_mmu_pre_destroy_vm(struct kvm* kvm);

void kvm_mmu_set_ept_masks(bool has_ad_bits, bool has_exec_only);
void kvm_mmu_set_mmio_spte_mask(u64 mmio_value, u64 mmio_mask, 
	u64 access_mask);

static u64 rsvd_bits(int s, int e) {
	if (e < s)
		return 0;

	return ((2ULL << (e - s)) - 1) << s;
}

bool __kvm_mmu_prepare_zap_page(struct kvm* kvm,
	struct kvm_mmu_page* sp,
	PLIST_ENTRY invalid_list,
	int* nr_zapped);

static inline u8 kvm_get_shadow_phys_bits(void) {
	return cpuid_eax(0x80000008) & 0xff;
}

void kvm_mmu_x86_module_init(void);

extern u8 shadow_phys_bits;

static inline gfn_t kvm_mmu_max_gfn(void) {
	/*
	* Note that this uses the host MAXPHYADDR, not the guest's.
	* EPT/NPT cannot support GPAs that would exceed host.MAXPHYADDR;
	* assuming KVM is running on bare metal, guest accesses beyond
	* host.MAXPHYADDR will hit a #PF(RSVD) and never cause a vmexit
	* (either EPT Violation/Misconfig or #NPF), and so KVM will never
	* install a SPTE for such addresses. If KVM is running as a VM 
	* itself, on the other hand, it might see a MAXPHYADDR that is less
	* than hardware's real MAXPHYADDR. Using the host MAXPHYADDR
	* disallows such SPTEs entirely and simplifies the TDP MMU.
	*/
	int max_gpa_bits = tdp_enabled ? shadow_phys_bits : 52;
	
	return (1ULL << (max_gpa_bits - PAGE_SHIFT)) - 1;
}

#ifdef AMD64
extern bool tdp_mmu_enabled;
#else
#define tdp_mmu_enabled false
#endif

static inline bool kvm_shadow_root_allocated(struct kvm* kvm) {
	/*
	* Read shadow_root_allocated before related pointers. Hence, threads
	* reading shadow_root_allocated in any lock context are guaranteed to 
	* see the pointers. 
	*/
	UNREFERENCED_PARAMETER(kvm);
	return FALSE;
}

static inline bool kvm_memslots_have_rmaps(struct kvm* kvm) {
	return !tdp_mmu_enabled || kvm_shadow_root_allocated(kvm);
}

static inline gfn_t gfn_to_index(gfn_t gfn, gfn_t base_gfn, int level) {
	/* KVM_HPAGE_GFN_SHIFT(PG_LEVEL_4K) must be 0.*/
	return (gfn >> KVM_HPAGE_GFN_SHIFT(level)) -
		(base_gfn >> KVM_HPAGE_GFN_SHIFT(level));
}

static inline ULONG_PTR __kvm_mmu_slot_lpages(struct kvm_memory_slot* slot,
	ULONG_PTR npages, int level) {
	return gfn_to_index(slot->base_gfn + npages - 1,
		slot->base_gfn, level) + 1;
}

static inline void kvm_update_page_stats(struct kvm* kvm, int level, int count) {
	InterlockedAdd64(&kvm->stat.pages[level - 1], count);
}