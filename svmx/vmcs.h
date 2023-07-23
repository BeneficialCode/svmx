#pragma once

struct vmcs_hdr {
	u32 revision_id : 31;
	u32 shadow_vmcs : 1;
};

struct vmcs {
	struct vmcs_hdr hdr;
	u32 abort;
	char data[];
};

/*
 * vmcs_host_state tracks registers that are loaded from the VMCS on VMEXIT
 * and whose values change infrequently, but are not constant.  I.e. this is
 * used as a write-through cache of the corresponding VMCS fields.
 */
struct vmcs_host_state {
	unsigned long cr3;	/* May not match real cr3 */
	unsigned long cr4;	/* May not match real cr4 */
	unsigned long gs_base;
	unsigned long fs_base;
	unsigned long rsp;

	u16           fs_sel, gs_sel, ldt_sel;
#ifdef _WIN64
	u16           ds_sel, es_sel;
#endif
};

struct vmcs_controls_shadow {
	u32 vm_entry;
	u32 vm_exit;
	u32 pin;
	u32 exec;
	u32 secondary_exec;
	u64 tertiary_exec;
};

/*
 * Track a VMCS that may be loaded on a certain CPU. If it is (cpu!=-1), also
 * remember whether it was VMLAUNCHed, and maintain a linked list of all VMCSs
 * loaded on this CPU (so we can clear them if the CPU goes down).
 */
struct loaded_vmcs {
	struct vmcs* vmcs;
	struct vmcs* shadow_vmcs;
	int cpu;
	bool launched;
	bool nmi_known_unmasked;
	bool hv_timer_soft_disabled;
	/* Support for vnmi-less CPUs */
	int soft_vnmi_blocked;
	s64 vnmi_blocked_time;
	RTL_BITMAP msr_bitmap;
	struct vmcs_host_state host_state;
	struct vmcs_controls_shadow controls_shadow;
};