#pragma once
#include "hvpp/config.h"
#include "hvpp/vcpu.h"
#include "hvpp/vmexit.h"
#include "hvpp/vmexit/vmexit_stats.h"
#include "hvpp/vmexit/vmexit_dbgbreak.h"
#include "hvpp/vmexit/vmexit_passthrough.h"

using namespace ia32;
using namespace hvpp;

class vmexit_custom_handler
  : public vmexit_passthrough_handler
{
  public:
    using base_type = vmexit_passthrough_handler;

    void setup(vcpu_t& vp) noexcept override;

    void handle_execute_cpuid(vcpu_t& vp) noexcept override;
    void handle_execute_vmcall(vcpu_t& vp) noexcept override;
    void handle_ept_violation(vcpu_t& vp) noexcept override;

  private:
    struct per_vcpu_data
    {
      pa_t page_read;
      pa_t page_exec;
    };

    per_vcpu_data data_[32];
};
