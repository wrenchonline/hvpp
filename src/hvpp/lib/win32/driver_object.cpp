#include "driver_object.h"

#include "../device.h"

#include <ntddk.h>

#include <algorithm>

#ifdef min
# undef min
#endif

//
// Macro to extract access out of the device io control code
//
#define ACCESS_FROM_CTL_CODE(ctrlCode)  (((ULONG)(ctrlCode & 0x0000c000)) >> 14)

namespace driver_object
{
  namespace detail
  {
    NTSTATUS dispatch(PDEVICE_OBJECT DeviceObject, PIRP Irp) noexcept
    {
      NTSTATUS Status = STATUS_SUCCESS;
      PIO_STACK_LOCATION IoStackLocation;
      device* CppDeviceObject;

      IoStackLocation = IoGetCurrentIrpStackLocation(Irp);
      CppDeviceObject = *static_cast<device**>(DeviceObject->DeviceExtension);
      switch (IoStackLocation->MajorFunction)
      {
        case IRP_MJ_CREATE:
          CppDeviceObject->on_create();
          break;

        case IRP_MJ_CLOSE:
          CppDeviceObject->on_close();
          break;

        case IRP_MJ_DEVICE_CONTROL:
          {
            PVOID Buffer             = Irp->AssociatedIrp.SystemBuffer;
            ULONG InputBufferLength  = IoStackLocation->Parameters.DeviceIoControl.InputBufferLength;
            ULONG OutputBufferLength = IoStackLocation->Parameters.DeviceIoControl.OutputBufferLength;
            ULONG IoControlCode      = IoStackLocation->Parameters.DeviceIoControl.IoControlCode;

            ULONG BufferLength = 0;

            switch (ACCESS_FROM_CTL_CODE(IoControlCode))
            {
              case FILE_READ_ACCESS:
                BufferLength = InputBufferLength;
                break;

              case FILE_WRITE_ACCESS:
                BufferLength = OutputBufferLength;
                break;

              case FILE_READ_ACCESS | FILE_WRITE_ACCESS:
                BufferLength = std::min(InputBufferLength, OutputBufferLength);
                break;
            }

            if (!BufferLength)
            {
              Buffer = nullptr;
            }

            CppDeviceObject->on_ioctl(IoControlCode, Buffer, BufferLength);

            if (ACCESS_FROM_CTL_CODE(IoControlCode) & FILE_WRITE_ACCESS)
            {
              Irp->IoStatus.Information = BufferLength;
            }
          }
          break;

        default:
          break;
      }

      Irp->IoStatus.Status = Status;
      IoCompleteRequest(Irp, IO_NO_INCREMENT);
      return Status;
    }
  }

  PDRIVER_OBJECT nt_driver_object = nullptr;

  void driver_object::initialize(void* object) noexcept
  {
    PDRIVER_OBJECT DriverObject = (PDRIVER_OBJECT)object;
    nt_driver_object = DriverObject;

    DriverObject->MajorFunction[IRP_MJ_CREATE]         = &detail::dispatch;
    DriverObject->MajorFunction[IRP_MJ_CLOSE]          = &detail::dispatch;
    DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = &detail::dispatch;
  }

  void driver_object::destroy() noexcept
  {
    PDRIVER_OBJECT DriverObject = (PDRIVER_OBJECT)nt_driver_object;

    DriverObject->MajorFunction[IRP_MJ_CREATE]         = nullptr;
    DriverObject->MajorFunction[IRP_MJ_CLOSE]          = nullptr;
    DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = nullptr;

    nt_driver_object = nullptr;
  }

  void* get() noexcept
  {
    return nt_driver_object;
  }
}
