#include "pch.h"
#include "mmu.h"

// ��ʼ�� mtrr ����
void kvm_vcpu_mtrr_init(struct kvm_vcpu* vcpu) {
	InitializeListHead(&vcpu->arch.mtrr_state.head);
}