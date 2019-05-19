#pragma once
#include <wdm.h>
typedef struct _SAMPLEDEVICE_EXTENSION {
	PDEVICE_OBJECT NextDeviceObject;

	unsigned char* pInternalStringBuffer; // Buffer to store the string passed from application
	ULONG dwStringLength;				  // Size of the buffer
} SAMPLEDEVICE_EXTENSION, * PSAMPLEDEVICE_EXTENSION;
#pragma once
