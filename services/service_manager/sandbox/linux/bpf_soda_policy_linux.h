// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_SERVICE_MANAGER_SANDBOX_LINUX_BPF_SODA_POLICY_LINUX_H_
#define SERVICES_SERVICE_MANAGER_SANDBOX_LINUX_BPF_SODA_POLICY_LINUX_H_

#include "sandbox/linux/bpf_dsl/bpf_dsl.h"
#include "services/service_manager/sandbox/linux/bpf_base_policy_linux.h"

namespace service_manager {

// The process policy for the sandboxed utility process that loads the Speech
// On-Device API (SODA). This policy allows the syscalls used by the libsoda.so
// binary to transcribe audio into text.
class SERVICE_MANAGER_SANDBOX_EXPORT SodaProcessPolicy : public BPFBasePolicy {
 public:
  SodaProcessPolicy();
  ~SodaProcessPolicy() override;

  sandbox::bpf_dsl::ResultExpr EvaluateSyscall(
      int system_call_number) const override;

 private:
  DISALLOW_COPY_AND_ASSIGN(SodaProcessPolicy);
};

}  // namespace service_manager

#endif  // SERVICES_SERVICE_MANAGER_SANDBOX_LINUX_BPF_SODA_POLICY_LINUX_H_
