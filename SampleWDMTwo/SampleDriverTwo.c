#include <wdm.h>
#include "SampleDriverTwo.h"

#define DEVICE_NAME L"\\Device\\WDM_DEVICESTACK_DEVICE"
#define SYMBOLIC_NAME L"\\DosDevices\\WDM_DEVICESTACK"

NTSTATUS SampleCreateIRPDispatch(IN PDEVICE_OBJECT SampleDevObj, IN PIRP Irp) {
	NTSTATUS ntStatus = STATUS_SUCCESS;

	UNREFERENCED_PARAMETER(SampleDevObj);

	DbgPrintEx(DPFLTR_IHVDRIVER_ID, 0, "CreateIRPDispatch started");

	Irp->IoStatus.Status = ntStatus; // Assign Success for now
	Irp->IoStatus.Information = 0;
	IoCompleteRequest(Irp, IO_NO_INCREMENT);

	DbgPrintEx(DPFLTR_IHVDRIVER_ID, 0, "CreateIRPDispatch done");

	return ntStatus;
}




NTSTATUS SampleReadIRPDispatch(IN PDEVICE_OBJECT SampleDevObj, IN PIRP Irp) {
	NTSTATUS ntStatus = STATUS_SUCCESS;
	PSAMPLEDEVICE_EXTENSION pDeviceExtension;
	ULONG dwReturnSize = 0;
	PIO_STACK_LOCATION pStack = IoGetCurrentIrpStackLocation(Irp);

	pDeviceExtension = SampleDevObj->DeviceExtension;

	// Calculate the smaller size b/w the length of string stored by the device and length of string requested by the application
	// We need the smaller of the 
	dwReturnSize = (pDeviceExtension->dwStringLength > pStack->Parameters.Read.Length) ? pStack->Parameters.Read.Length : pDeviceExtension->dwStringLength;

	// If return size is not 0, ie must return something
	if (dwReturnSize) {
		memcpy(Irp->AssociatedIrp.SystemBuffer, pDeviceExtension->pInternalStringBuffer, dwReturnSize);
	}

	Irp->IoStatus.Status = ntStatus;

	// Record the length of string processed
	Irp->IoStatus.Information = dwReturnSize;

	IoCompleteRequest(Irp, IO_NO_INCREMENT);

	return ntStatus;
}




NTSTATUS SampleWriteIRPDispatch(IN PDEVICE_OBJECT SampleDevObj, IN PIRP Irp) {
	NTSTATUS ntStatus = STATUS_SUCCESS;
	PSAMPLEDEVICE_EXTENSION pDeviceExtension;
	PIO_STACK_LOCATION pStack = IoGetCurrentIrpStackLocation(Irp);

	pDeviceExtension = (PSAMPLEDEVICE_EXTENSION)SampleDevObj->DeviceExtension;

	// If a buffer already exists, first clean out the buffer
	if (pDeviceExtension->pInternalStringBuffer) {
		ExFreePool(pDeviceExtension->pInternalStringBuffer);
		pDeviceExtension->pInternalStringBuffer = NULL;
		pDeviceExtension->dwStringLength = 0;
	}

	pDeviceExtension->dwStringLength = pStack->Parameters.Write.Length;

	// If length of new string is greater than 0
	if (pDeviceExtension->dwStringLength) {

		// Allocate new memory for the string 
		pDeviceExtension->pInternalStringBuffer = ExAllocatePool(NonPagedPool, pDeviceExtension->dwStringLength);

		// Copy the string from the application to the device
		memcpy(pDeviceExtension->pInternalStringBuffer, Irp->AssociatedIrp.SystemBuffer, pDeviceExtension->dwStringLength);
	}

	Irp->IoStatus.Status = ntStatus;

	// Record the length of the string processed
	Irp->IoStatus.Information = pDeviceExtension->dwStringLength;
	IoCompleteRequest(Irp, IO_NO_INCREMENT);

	return ntStatus;
}




NTSTATUS SampleCloseIRPDispatch(IN PDEVICE_OBJECT SampleDevObj, IN PIRP Irp) {
	NTSTATUS ntStatus = STATUS_SUCCESS;

	PSAMPLEDEVICE_EXTENSION pDeviceExtension;

	pDeviceExtension = (PSAMPLEDEVICE_EXTENSION)SampleDevObj->DeviceExtension;

	// If buffer exists, free the buffer
	if (pDeviceExtension->dwStringLength) {
		ExFreePool(pDeviceExtension->pInternalStringBuffer);
		pDeviceExtension->pInternalStringBuffer = NULL;
		pDeviceExtension->dwStringLength = 0;
	}

	Irp->IoStatus.Status = ntStatus;
	Irp->IoStatus.Information = 0;
	IoCompleteRequest(Irp, IO_NO_INCREMENT);

	return ntStatus;
}




void SampleDriverUnload(IN PDRIVER_OBJECT DriverObject) {
	UNREFERENCED_PARAMETER(DriverObject);

	DbgPrintEx(DPFLTR_IHVDRIVER_ID, 0, "Driver Unloading");
}




NTSTATUS SampleAddDevice(IN PDRIVER_OBJECT DriverObject, IN PDEVICE_OBJECT PhysicalDeviceObject) {
	PSAMPLEDEVICE_EXTENSION pSampleDeviceExtension = NULL;
	NTSTATUS ntStatus = STATUS_UNSUCCESSFUL;
	PDEVICE_OBJECT SampleDeviceObject;

	DbgPrintEx(DPFLTR_IHVDRIVER_ID, 0, "SampleAddDevice Executing");

	// Creating a device object, attach it to SampleDeviceObject
	ntStatus = IoCreateDevice(DriverObject, sizeof(SAMPLEDEVICE_EXTENSION), NULL, FILE_DEVICE_UNKNOWN, FILE_AUTOGENERATED_DEVICE_NAME, FALSE, &SampleDeviceObject);

	if (!NT_SUCCESS(ntStatus)) {
		DbgPrintEx(DPFLTR_IHVDRIVER_ID, 0, "SampleAddDevice Failed");
		return ntStatus;
	}

	// Create Symbolic Link of this device object
	// Symbolic Links allows for an user application to access this device object
	UNICODE_STRING devName;
	UNICODE_STRING symName;

	RtlInitUnicodeString(&devName, DEVICE_NAME);
	RtlInitUnicodeString(&symName, SYMBOLIC_NAME);

	IoCreateSymbolicLink(&symName, &devName);

	SampleDeviceObject->Flags |= DO_BUFFERED_IO;
	/////////////////////////////////////////////

	// Stack the newly created device object onto the device stack
	pSampleDeviceExtension = SampleDeviceObject->DeviceExtension;
	RtlZeroMemory(pSampleDeviceExtension, sizeof(SAMPLEDEVICE_EXTENSION));
	// Newly created device object will go on top of the device object created by the bus driver: PhysicalDeviceObject
	pSampleDeviceExtension->NextDeviceObject = IoAttachDeviceToDeviceStack(SampleDeviceObject, PhysicalDeviceObject);

	SampleDeviceObject->Flags &= ~DO_DEVICE_INITIALIZING;
	return ntStatus;
}




NTSTATUS SamplePnPIRPDispatch(IN PDEVICE_OBJECT SampleDeviceObj, IN PIRP Irp) {
	NTSTATUS ntStatus = STATUS_NOT_SUPPORTED;
	PSAMPLEDEVICE_EXTENSION pDeviceExtension;
	PDEVICE_OBJECT pNextDeviceObj;
	PIO_STACK_LOCATION pStack = IoGetCurrentIrpStackLocation(Irp);
	pDeviceExtension = (PSAMPLEDEVICE_EXTENSION)SampleDeviceObj->DeviceExtension;
	pNextDeviceObj = pDeviceExtension->NextDeviceObject;
	UNICODE_STRING symName;

	switch (pStack->MinorFunction) {
	case IRP_MN_REMOVE_DEVICE:
		// Delete the symbolic link to this device object
		RtlInitUnicodeString(&symName, SYMBOLIC_NAME);
		IoDeleteSymbolicLink(&symName);

		IoDetachDevice(pNextDeviceObj); // Detach current device object from device stack
		IoDeleteDevice(SampleDeviceObj); // Delete current device object
		break;
	default:
		break;
	}

	IoSkipCurrentIrpStackLocation(Irp);
	ntStatus = IoCallDriver(pNextDeviceObj, Irp); // Send Irp down the device stack
	return ntStatus;
}




// Called when the driver is loaded onto memory
NTSTATUS DriverEntry(IN PDRIVER_OBJECT DriverObject, IN PUNICODE_STRING RegistryPath) {
	UNREFERENCED_PARAMETER(RegistryPath);

	DbgPrintEx(DPFLTR_IHVDRIVER_ID, 0, "Driver Loading");

	// Gets called when the driver is unloaded from memory
	DriverObject->DriverUnload = SampleDriverUnload;

	// Gets called when the device object is created and added to the device stack
	DriverObject->DriverExtension->AddDevice = SampleAddDevice;

	// Callback function to manage PnP IRP
	DriverObject->MajorFunction[IRP_MJ_PNP] = SamplePnPIRPDispatch;

	return STATUS_SUCCESS;
}
