#include "pch.h"
#include "ntexapi.h"
#include "vmx.h"
#include "svm.h"

extern KMUTEX vendor_module_lock;
extern struct vmcs** vmxarea;
extern struct vmcs** current_vmcs;
extern bool* hardware_enabled;

DRIVER_UNLOAD DriverUnload;
DRIVER_DISPATCH DriverDeviceControl;
DRIVER_DISPATCH DriverCreateClose, DriverShutdown;
bool g_vmx_init = FALSE;
bool g_svm_init = FALSE;

NTSTATUS DriverEntry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING RegistryPath);

VOID DriverUnload(PDRIVER_OBJECT DriverObject) {
	if (g_vmx_init) {
		vmx_exit();
	}
	else if (g_svm_init) {
		svm_exit();
	}
	if (vmxarea != NULL) {
		ExFreePool(vmxarea);
	}
	if (current_vmcs != NULL) {
		ExFreePool(current_vmcs);
	}
	UNICODE_STRING linkName = RTL_CONSTANT_STRING(L"\\??\\KVM");
	IoDeleteSymbolicLink(&linkName);
	IoDeleteDevice(DriverObject->DeviceObject);

}

NTSTATUS DriverEntry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING RegistryPath) {
	UNREFERENCED_PARAMETER(RegistryPath);
	NTSTATUS status = STATUS_UNSUCCESSFUL;

	DriverObject->DriverUnload = DriverUnload;
	DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = DriverDeviceControl;
	DriverObject->MajorFunction[IRP_MJ_CREATE] = DriverCreateClose;
	DriverObject->MajorFunction[IRP_MJ_CLOSE] = DriverCreateClose;
	DriverObject->MajorFunction[IRP_MJ_SHUTDOWN] = DriverShutdown;

	KeInitializeMutex(&vendor_module_lock, 0);
	
	UNICODE_STRING devName = RTL_CONSTANT_STRING(L"\\Device\\KVM");
	PDEVICE_OBJECT DeviceObject;

	status = IoCreateDevice(DriverObject, 0, &devName,
		FILE_DEVICE_UNKNOWN, 0, TRUE, &DeviceObject);
	if (!NT_SUCCESS(status))
		return status;

	UNICODE_STRING linkName = RTL_CONSTANT_STRING(L"\\??\\KVM");
	status = IoCreateSymbolicLink(&linkName, &devName);
	if (!NT_SUCCESS(status)) {
		IoDeleteDevice(DeviceObject);
		return status;
	}

	ULONG count = KeQueryActiveProcessorCount(0);
	hardware_enabled = ExAllocatePoolZero(NonPagedPool, count * sizeof(bool),
		DRIVER_TAG);
	if (hardware_enabled == NULL) {
		status = STATUS_NO_MEMORY;
		IoDeleteSymbolicLink(&linkName);
		IoDeleteDevice(DeviceObject);
		return status;
	}
	
	int cpuInfo[4];
	CpuIdEx(cpuInfo, 0, 0);
	char brand[13] = { 0 };
	memcpy_s(brand, 4, &cpuInfo[1], 4);
	memcpy_s(brand + 4, 4,&cpuInfo[3], 4);
	memcpy_s(brand + 8, 4, &cpuInfo[2], 4);
	if (strcmp(brand,"GenuineIntel") == 0) {
		
		vmxarea = ExAllocatePoolZero(NonPagedPool, count * sizeof(struct vmcs*),
			DRIVER_TAG);
		if (vmxarea == NULL) {
			status = STATUS_NO_MEMORY;
			IoDeleteSymbolicLink(&linkName);
			IoDeleteDevice(DeviceObject);
			return status;
		}
		current_vmcs = ExAllocatePoolZero(NonPagedPool, count * sizeof(struct vmcs*),
			DRIVER_TAG);
		if (current_vmcs == NULL) {
			status = STATUS_NO_MEMORY;
			ExFreePool(vmxarea);
			vmxarea = NULL;
			IoDeleteSymbolicLink(&linkName);
			IoDeleteDevice(DeviceObject);
			return status;
		}
		// module initialize
		status = vmx_init();
		if (NT_SUCCESS(status))
			g_vmx_init = TRUE;
	}
	else if (strcmp(brand, "AuthenticAMD") == 0) {
		status = svm_init();
		if (NT_SUCCESS(status))
			g_svm_init = TRUE;
	}
	else {
		status = STATUS_NOT_SUPPORTED;
	}

	if (!NT_SUCCESS(status)) {
		ExFreePool(vmxarea);
		ExFreePool(current_vmcs);
		vmxarea = NULL;
		current_vmcs = NULL;
		IoDeleteSymbolicLink(&linkName);
		IoDeleteDevice(DeviceObject);
	}

	return status;
}

// kvm ioctl entry
NTSTATUS DriverDeviceControl(PDEVICE_OBJECT DeviceObject, PIRP Irp) {
	UNREFERENCED_PARAMETER(DeviceObject);
	NTSTATUS status = STATUS_SUCCESS;

	ULONG ioctl = IoGetCurrentIrpStackLocation(Irp)->Parameters.DeviceIoControl.IoControlCode;
	switch (ioctl)
	{
	case KVM_GET_API_VERSION:

		break;

	case KVM_CREATE_VM:
		// create virtual machine
		status = kvm_dev_ioctl_create_vm(0);
		break;

	case KVM_CHECK_EXTENSION:

		break;

	case KVM_GET_VCPU_MMPA_SIZE:

		break;
	case KVM_TRACE_ENABLE:
	case KVM_TRACE_PAUSE:
	case KVM_TRACE_DISABLE:
		status = STATUS_NOT_SUPPORTED;
		break;

	// kvm_vm_ioctl
	case KVM_CREATE_VCPU:
		status = kvm_vm_ioctl_create_vcpu(NULL, 0);
		break;
	default:

		break;
	}
	Irp->IoStatus.Status = status;
	Irp->IoStatus.Information = 0;
	IoCompleteRequest(Irp, IO_NO_INCREMENT);
	return status;
}

NTSTATUS CompleteRequest(PIRP Irp, NTSTATUS status, ULONG_PTR info) {
	Irp->IoStatus.Status = status;
	Irp->IoStatus.Information = info;
	IoCompleteRequest(Irp, IO_NO_INCREMENT);
	return status;
}

NTSTATUS DriverCreateClose(PDEVICE_OBJECT DeviceObject, PIRP Irp) {
	UNREFERENCED_PARAMETER(DeviceObject);
	return CompleteRequest(Irp, STATUS_SUCCESS, 0);
}

NTSTATUS DriverShutdown(PDEVICE_OBJECT DeviceObject, PIRP Irp) {
	UNREFERENCED_PARAMETER(DeviceObject);
	return CompleteRequest(Irp, STATUS_SUCCESS, 0);
}