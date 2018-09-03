#pragma once

namespace driver_object
{
  void initialize(void* driver_object_ptr) noexcept;
  void destroy() noexcept;

  void* get() noexcept;
}
