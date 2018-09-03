#include "vmexit_stats.h"

#include "hvpp/vcpu.h"

#include "lib/log.h"
#include "lib/mp.h" // mp::cpu_index()

#include <iterator> // std::size()

#define hvpp_trace_if_enabled(format, ...)                        \
  do                                                              \
  {                                                               \
    if (vmexit_trace_bitmap_.test(static_cast<int>(exit_reason))) \
    {                                                             \
      hvpp_trace(format, __VA_ARGS__);                            \
    }                                                             \
  } while (0)

namespace hvpp {

auto vmexit_stats_handler::initialize() noexcept -> error_code_t
{
  terminated_vcpu_count_ = 0;

  //
  // Allocate memory for statistics (per VCPU).
  //
  storage_ = new vmexit_stats_storage_t[mp::cpu_count()];

  if (!storage_)
  {
    return make_error_code_t(std::errc::not_enough_memory);
  }

  memset(storage_, 0, sizeof(*storage_) * mp::cpu_count());

  //
  // Uncomment this to trace all VM-exit reasons.
  // Tracing of specific VM-exit reasons can be enabled/disabled
  // via this bitmap.
  //
  // vmexit_trace_bitmap_.set();
  //
  // Example of disabling trace of "exception or NMI" VM-exit
  // reason:
  //
  // vmexit_trace_bitmap_.clear(int(vmx::exit_reason::exception_or_nmi));
  //

  return error_code_t{};
}

void vmexit_stats_handler::destroy() noexcept
{
  if (storage_)
  {
    //
    // Free the memory.
    //
    delete[] storage_;
  }
}

void vmexit_stats_handler::handle(vcpu_t& vp) noexcept
{
  auto  exit_reason = vp.exit_reason();
  auto& stats       = storage_[mp::cpu_index()];

  stats.vmexit[static_cast<int>(exit_reason)] += 1;

  switch (exit_reason)
  {
    case vmx::exit_reason::exception_or_nmi:
      switch (vp.exit_interrupt_info().type())
      {
        case vmx::interrupt_type::hardware_exception:
        case vmx::interrupt_type::software_exception:
          stats.expt_vector[static_cast<int>(vp.exit_interrupt_info().vector())] += 1;

          hvpp_trace_if_enabled(
            "exit_reason::exception_or_nmi: %s",
            exception_vector_to_string(vp.exit_interrupt_info().vector()));
          break;
      }
      break;

    case vmx::exit_reason::execute_cpuid:
      if (vp.exit_context().eax < (0x0000'0000u + vmexit_stats_storage_t::cpuid_0_max))
      {
        stats.cpuid_0[vp.exit_context().eax] += 1;
      }
      else if (vp.exit_context().eax >= 0x8000'0000u &&
               vp.exit_context().eax < (0x8000'0000u + vmexit_stats_storage_t::cpuid_8_max))
      {
        stats.cpuid_8[vp.exit_context().eax - 0x8000'0000u] += 1;
      }
      else
      {
        stats.cpuid_other += 1;
      }

      hvpp_trace_if_enabled("exit_reason::execute_cpuid: 0x%08x", vp.exit_context().eax);
      break;

    case vmx::exit_reason::execute_invd:
      hvpp_trace_if_enabled("exit_reason::execute_invd");
      break;

    case vmx::exit_reason::execute_invlpg:
      hvpp_trace_if_enabled("exit_reason::execute_invlpg: 0x%p", vp.exit_qualification().linear_address);
      break;

    case vmx::exit_reason::execute_rdtsc:
      hvpp_trace_if_enabled("exit_reason::execute_rdtsc");
      break;

    case vmx::exit_reason::mov_cr:
      switch (vp.exit_qualification().mov_cr.access_type)
      {
        case vmx::exit_qualification_mov_cr_t::access_to_cr:
          stats.mov_to_cr[vp.exit_qualification().mov_cr.cr_number] += 1;

          hvpp_trace_if_enabled(
            "exit_reason::mov_cr: (to_cr%u) 0x%p",
            vp.exit_qualification().mov_cr.cr_number,
            vp.exit_context().gp_register[vp.exit_qualification().mov_cr.gp_register]);
          break;

        case vmx::exit_qualification_mov_cr_t::access_from_cr:
          stats.mov_from_cr[vp.exit_qualification().mov_cr.cr_number] += 1;

          hvpp_trace_if_enabled(
            "exit_reason::mov_cr: (from_cr%u) 0x%p",
            vp.exit_qualification().mov_cr.cr_number,
            vp.exit_context().gp_register[vp.exit_qualification().mov_cr.gp_register]);
          break;

        case vmx::exit_qualification_mov_cr_t::access_clts:
          stats.clts += 1;

          hvpp_trace_if_enabled("exit_reason::mov_cr: (clts)");
          break;

        case vmx::exit_qualification_mov_cr_t::access_lmsw:
          stats.lmsw += 1;

          hvpp_trace_if_enabled("exit_reason::mov_cr: (lmsw)");
          break;
      }
      break;

    case vmx::exit_reason::mov_dr:
      switch (vp.exit_qualification().mov_dr.access_type)
      {
        case vmx::exit_qualification_mov_dr_t::access_to_dr:
          stats.mov_to_dr[vp.exit_qualification().mov_dr.dr_number] += 1;

          hvpp_trace_if_enabled(
            "exit_reason::mov_dr: (to_dr%u) 0x%p",
            vp.exit_qualification().mov_dr.dr_number,
            vp.exit_context().gp_register[vp.exit_qualification().mov_dr.gp_register]);
          break;

        case vmx::exit_qualification_mov_dr_t::access_from_dr:
          stats.mov_from_dr[vp.exit_qualification().mov_dr.dr_number] += 1;

          hvpp_trace_if_enabled(
            "exit_reason::mov_dr: (from_dr%u) 0x%p",
            vp.exit_qualification().mov_dr.dr_number,
            vp.exit_context().gp_register[vp.exit_qualification().mov_dr.gp_register]);
          break;
      }
      break;

    case vmx::exit_reason::execute_io_instruction:
      switch (vp.exit_qualification().io_instruction.access_type)
      {
        case vmx::exit_qualification_io_instruction_t::access_out:
          stats.io_out[vp.exit_qualification().io_instruction.port_number] += 1;

          hvpp_trace_if_enabled(
            "exit_reason::execute_io_instruction: out 0x%04x",
            vp.exit_qualification().io_instruction.port_number);
          break;

        case vmx::exit_qualification_io_instruction_t::access_in:
          stats.io_in[vp.exit_qualification().io_instruction.port_number] += 1;

          hvpp_trace_if_enabled(
            "exit_reason::execute_io_instruction: in 0x%04x",
            vp.exit_qualification().io_instruction.port_number);
          break;
      }
      break;

    case vmx::exit_reason::execute_rdmsr:
      if (vp.exit_context().ecx <= 0x0000'1fffu)
      {
        stats.rdmsr_0[vp.exit_context().ecx] += 1;
      }
      else if (vp.exit_context().ecx >= 0xc000'0000u &&
               vp.exit_context().ecx <= 0xc000'1fffu)
      {
        stats.rdmsr_c[vp.exit_context().ecx - 0xc000'0000u] += 1;
      }
      else
      {
        stats.rdmsr_other += 1;
      }
      hvpp_trace_if_enabled("exit_reason::execute_rdmsr: 0x%08x", vp.exit_context().ecx);
      break;

    case vmx::exit_reason::execute_wrmsr:
      if (vp.exit_context().ecx <= 0x0000'1fffu)
      {
        stats.wrmsr_0[vp.exit_context().ecx] += 1;
      }
      else if (vp.exit_context().ecx >= 0xc000'0000u &&
               vp.exit_context().ecx <= 0xc000'1fffu)
      {
        stats.wrmsr_c[vp.exit_context().ecx - 0xc000'0000u] += 1;
      }
      else
      {
        stats.wrmsr_other += 1;
      }

      hvpp_trace_if_enabled("exit_reason::execute_wrmsr: 0x%08x", vp.exit_context().ecx);
      break;

    case vmx::exit_reason::gdtr_idtr_access:
      stats.gdtr_idtr[vp.exit_instruction_info().gdtr_idtr_access.instruction] += 1;

      hvpp_trace_if_enabled(
        "exit_reason::gdtr_idtr_access: %s",
        vmx::instruction_info_gdtr_idtr_to_string(vp.exit_instruction_info().gdtr_idtr_access.instruction));
      break;

    case vmx::exit_reason::ldtr_tr_access:
      stats.ldtr_tr[vp.exit_instruction_info().ldtr_tr_access.instruction] += 1;

      hvpp_trace_if_enabled(
        "exit_reason::ldtr_tr_access: %s",
        vmx::instruction_info_ldtr_tr_to_string(vp.exit_instruction_info().ldtr_tr_access.instruction));
      break;

    case vmx::exit_reason::ept_violation:
      //
      // Do not trace.
      //
      break;

    case vmx::exit_reason::execute_rdtscp:
      hvpp_trace_if_enabled("exit_reason::execute_rdtscp");
      break;

    case vmx::exit_reason::execute_wbinvd:
      hvpp_trace_if_enabled("exit_reason::execute_wbinvd");
      break;

    case vmx::exit_reason::execute_xsetbv:
      hvpp_trace_if_enabled("exit_reason::execute_xsetbv: [0x%08x] -> %p",
        vp.exit_context().ecx,
        vp.exit_context().rdx << 32 | vp.exit_context().rax);
      break;

    case vmx::exit_reason::execute_invpcid:
      hvpp_trace_if_enabled("exit_reason::execute_invpcid");
      break;
  }
}

void vmexit_stats_handler::dump() noexcept
{
  //
  // Reset values.
  //
  memset(&storage_merged_, 0, sizeof(storage_merged_));

  //
  // Handler saves statistics separately for each VCPU.
  // We merge statistics from all VCPUs into the first
  // one (index 0).
  //
  for (uint32_t i = 0; i < mp::cpu_count(); ++i)
  {
    storage_merge(storage_merged_, storage_[i]);
  }

  //
  // Print merged statistics.
  // This is sum of statistics for each VCPU.
  //
  storage_dump(storage_merged_);
}

void vmexit_stats_handler::storage_merge(vmexit_stats_storage_t& lhs, const vmexit_stats_storage_t& rhs) const noexcept
{
#define STORAGE_MERGE_IMPL(name)                      \
  for (uint32_t i = 0; i < std::size(lhs.name); ++i)  \
  {                                                   \
    lhs.name[i] += rhs.name[i];                       \
  }

  STORAGE_MERGE_IMPL(vmexit);
  STORAGE_MERGE_IMPL(expt_vector);
  STORAGE_MERGE_IMPL(cpuid_0);
  STORAGE_MERGE_IMPL(cpuid_8);
  lhs.cpuid_other += rhs.cpuid_other;
  STORAGE_MERGE_IMPL(mov_from_cr);
  STORAGE_MERGE_IMPL(mov_to_cr);
  lhs.clts += rhs.clts;
  lhs.lmsw += rhs.lmsw;
  STORAGE_MERGE_IMPL(mov_from_dr);
  STORAGE_MERGE_IMPL(mov_to_dr);
  STORAGE_MERGE_IMPL(gdtr_idtr);
  STORAGE_MERGE_IMPL(ldtr_tr);
  STORAGE_MERGE_IMPL(io_in);
  STORAGE_MERGE_IMPL(io_out);
  STORAGE_MERGE_IMPL(rdmsr_0);
  STORAGE_MERGE_IMPL(rdmsr_c);
  lhs.rdmsr_other += rhs.rdmsr_other;
  STORAGE_MERGE_IMPL(wrmsr_0);
  STORAGE_MERGE_IMPL(wrmsr_c);
  lhs.wrmsr_other += rhs.wrmsr_other;

#undef STORAGE_MERGE_IMPL
}

void vmexit_stats_handler::storage_dump(const vmexit_stats_storage_t& storage_to_dump) const noexcept
{
  auto& stats = storage_to_dump;

  hvpp_info("VMEXIT statistics");
  for (uint32_t exit_reason_index = 0; exit_reason_index < std::size(stats.vmexit); ++exit_reason_index)
  {
    if (stats.vmexit[exit_reason_index] > 0)
    {
      hvpp_info("  %s: %u",
        vmx::exit_reason_to_string(static_cast<vmx::exit_reason>(exit_reason_index)),
        stats.vmexit[exit_reason_index]);

      switch (static_cast<vmx::exit_reason>(exit_reason_index))
      {
        case vmx::exit_reason::exception_or_nmi:
          for (uint32_t i = 0; i < std::size(stats.expt_vector); ++i)
          {
            const char* expt_vector_string = exception_vector_to_string(static_cast<exception_vector>(i));
            (void)(expt_vector_string);
            if (stats.expt_vector[i] > 0)
            {
              hvpp_info("    %s: %u", expt_vector_string, stats.expt_vector[i]);
            }
          }
          break;

        case vmx::exit_reason::execute_cpuid:
          for (uint32_t i = 0; i < std::size(stats.cpuid_0); ++i)
          {
            if (stats.cpuid_0[i] > 0)
            {
              hvpp_info("    0x%08x: %u", i, stats.cpuid_0[i]);
            }
          }

          for (uint32_t i = 0; i < std::size(stats.cpuid_8); ++i)
          {
            if (stats.cpuid_8[i] > 0)
            {
              hvpp_info("    0x%08x: %u", i + 0x8000'0000u, stats.cpuid_8[i]);
            }
          }

          if (stats.cpuid_other > 0)
          {
            hvpp_info("    0x(OTHER): %u", stats.cpuid_other);
          }
          break;

        case vmx::exit_reason::mov_cr:
          for (uint32_t i = 0; i < std::size(stats.mov_from_cr); ++i)
          {
            if (stats.mov_from_cr[i] > 0)
            {
              hvpp_info("    mov_from_cr[%i]: %u", i, stats.mov_from_cr[i]);
            }
          }

          for (uint32_t i = 0; i < std::size(stats.mov_to_cr); ++i)
          {
            if (stats.mov_to_cr[i] > 0)
            {
              hvpp_info("    mov_to_cr[%i]: %u", i, stats.mov_to_cr[i]);
            }
          }

          if (stats.lmsw > 0)
          {
            hvpp_info("    clts: %u", stats.clts);
          }

          if (stats.lmsw > 0)
          {
            hvpp_info("    lmsw: %u", stats.lmsw);
          }
          break;

        case vmx::exit_reason::mov_dr:
          for (uint32_t i = 0; i < std::size(stats.mov_from_dr); ++i)
          {
            if (stats.mov_from_dr[i] > 0)
            {
              hvpp_info("    mov_from_dr[%i]: %u", i, stats.mov_from_dr[i]);
            }
          }

          for (uint32_t i = 0; i < std::size(stats.mov_to_dr); ++i)
          {
            if (stats.mov_to_dr[i] > 0)
            {
              hvpp_info("    mov_to_dr[%i]: %u", i, stats.mov_to_dr[i]);
            }
          }
          break;

        case vmx::exit_reason::gdtr_idtr_access:
          for (uint32_t i = 0; i < std::size(stats.gdtr_idtr); ++i)
          {
            if (stats.gdtr_idtr[i] > 0)
            {
              hvpp_info("    %s: %u", vmx::instruction_info_gdtr_idtr_to_string(i), stats.gdtr_idtr[i]);
            }
          }
          break;

        case vmx::exit_reason::ldtr_tr_access:
          for (uint32_t i = 0; i < std::size(stats.ldtr_tr); ++i)
          {
            if (stats.ldtr_tr[i] > 0)
            {
              hvpp_info("    %s: %u", vmx::instruction_info_ldtr_tr_to_string(i), stats.ldtr_tr[i]);
            }
          }
          break;

        case vmx::exit_reason::execute_io_instruction:
          for (uint32_t i = 0; i < std::size(stats.io_in); ++i)
          {
            if (stats.io_in[i] > 0)
            {
              hvpp_info("    in (0x%04x): %u", i, stats.io_in[i]);
            }
          }

          for (uint32_t i = 0; i < std::size(stats.io_out); ++i)
          {
            if (stats.io_out[i] > 0)
            {
              hvpp_info("    out (0x%04x): %u", i, stats.io_out[i]);
            }
          }
          break;

        case vmx::exit_reason::execute_rdmsr:
          for (uint32_t i = 0; i < std::size(stats.rdmsr_0); ++i)
          {
            if (stats.rdmsr_0[i])
            {
              hvpp_info("    0x%08x: %u", i, stats.rdmsr_0[i]);
            }
          }

          for (uint32_t i = 0; i < std::size(stats.rdmsr_c); ++i)
          {
            if (stats.rdmsr_c[i])
            {
              hvpp_info("    0x%08x: %u", i + 0xc000'0000u, stats.rdmsr_c[i]);
            }
          }

          if (stats.rdmsr_other > 0)
          {
            hvpp_info("    (OTHER): %u", stats.rdmsr_other);
          }
          break;

        case vmx::exit_reason::execute_wrmsr:
          for (uint32_t i = 0; i < std::size(stats.wrmsr_0); ++i)
          {
            if (stats.wrmsr_0[i])
            {
              hvpp_info("    0x%08x: %u", i, stats.wrmsr_0[i]);
            }
          }

          for (uint32_t i = 0; i < std::size(stats.wrmsr_c); ++i)
          {
            if (stats.wrmsr_c[i])
            {
              hvpp_info("    0x%08x: %u", i + 0xc000'0000u, stats.wrmsr_c[i]);
            }
          }

          if (stats.wrmsr_other > 0)
          {
            hvpp_info("    (OTHER): %u", stats.wrmsr_other);
          }
          break;
      }
    }
  }
}

}
