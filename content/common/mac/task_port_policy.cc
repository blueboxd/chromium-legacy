// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/mac/task_port_policy.h"

#include <sys/sysctl.h>
#include <dlfcn.h>

#include "base/logging.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"

extern "C" {
int __sandbox_ms(const char* policy, int op, void* arg);
}

namespace content {

namespace {

// From AppleMobileFileIntegrity.kext`policy_syscall() on macOS 12.4 21F79.
enum AmfiStatusFlags {
  // Boot arg: `amfi_unrestrict_task_for_pid`.
  kAmfiOverrideUnrestrictedDebugging = 1 << 0,
  // Boot arg: `amfi_allow_any_signature`.
  kAmfiAllowInvalidSignatures = 1 << 1,
  // Boot arg: `amfi_get_out_of_my_way`.
  kAmfiAllowEverything = 1 << 2,
};

}  // namespace

bool MachTaskPortPolicy::AmfiIsAllowEverything() const {
  return amfi_status_retval == 0 && (amfi_status & kAmfiAllowEverything) != 0;
}

MachTaskPortPolicy GetMachTaskPortPolicy() {
  typedef int (*csr_check_Ptr)(uint32_t);
  static const csr_check_Ptr csr_check_FuncPtr =
      reinterpret_cast<csr_check_Ptr>(dlsym(((void *) -2), "csr_check"));
      
  MachTaskPortPolicy policy;
  // Undocumented MACF system call to Apple Mobile File Integrity.kext. In
  // macOS 12.4 21F79 (and at least back to macOS 12.0), this returns a
  // bitmask containing the AMFI status flags.
  policy.amfi_status_retval = __sandbox_ms("AMFI", 0x60, &policy.amfi_status);

  size_t capacity = 0;
  const char kBootArgs[] = "kern.bootargs";
  std::string boot_args;
  if (sysctlbyname(kBootArgs, nullptr, &capacity, nullptr, 0) == 0) {
    boot_args.resize(capacity);
    if (sysctlbyname(kBootArgs, boot_args.data(), &capacity, nullptr, 0) == 0) {
      policy.boot_args = ParseBootArgs(boot_args);
    } else {
      DPLOG(ERROR) << "sysctlbyname";
    }
  } else {
    DPLOG(ERROR) << "sysctlbyname capacity";
  }

  // From xnu-8019.80.24/bsd/sys/csr.h. Returns -1 with EPERM if the
  // operation is not allowed.
  const uint32_t CSR_ALLOW_KERNEL_DEBUGGER = 1 << 3;

  if(csr_check_FuncPtr) {
    errno = 0;
    policy.csr_kernel_debugger_retval = csr_check_FuncPtr(CSR_ALLOW_KERNEL_DEBUGGER);
    policy.csr_kernel_debugger_errno = errno;
  }
  return policy;
}

}  // namespace content
