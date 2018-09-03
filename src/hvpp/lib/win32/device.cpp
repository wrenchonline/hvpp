#include "../device.h"
#include "driver_object.h"

#include <ntddk.h>

void device::initialize() noexcept
{
  NTSTATUS       Status;
  UNICODE_STRING DeviceName = RTL_CONSTANT_STRING(L"\\Device\\hvpp");
  UNICODE_STRING DeviceLink = RTL_CONSTANT_STRING(L"\\DosDevices\\hvpp");
  PDRIVER_OBJECT DriverObject = (PDRIVER_OBJECT)driver_object::get();
  PDEVICE_OBJECT DeviceObject;

  Status = IoCreateDevice(DriverObject,
                          sizeof(device*),
                          &DeviceName,
                          FILE_DEVICE_UNKNOWN,
                          0,
                          FALSE,
                          &DeviceObject);

  device** CppDeviceObject = static_cast<device**>(DeviceObject->DeviceExtension);
  *CppDeviceObject = this;

  Status = IoCreateSymbolicLink(&DeviceLink, &DeviceName);

  device_object_ = DeviceObject;
}

void device::destroy() noexcept
{
  PDEVICE_OBJECT DeviceObject = (PDEVICE_OBJECT)device_object_;

  IoDeleteDevice(DeviceObject);
}
