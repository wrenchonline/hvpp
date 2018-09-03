#pragma once
#include "ia32/asm.h"

namespace debugger
{
  namespace detail
  {
    bool is_enabled() noexcept;
  }

  inline void breakpoint() noexcept
  { ia32_asm_int3(); }

  inline bool is_enabled() noexcept
  { return detail::is_enabled(); }
}
