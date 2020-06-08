// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_SERVICE_MANAGER_SANDBOX_LINUX_BPF_TTS_POLICY_LINUX_H_
#define SERVICES_SERVICE_MANAGER_SANDBOX_LINUX_BPF_TTS_POLICY_LINUX_H_

#include "sandbox/linux/bpf_dsl/bpf_dsl.h"
#include "services/service_manager/sandbox/export.h"
#include "services/service_manager/sandbox/linux/bpf_base_policy_linux.h"

namespace service_manager {

class SERVICE_MANAGER_SANDBOX_EXPORT TtsProcessPolicy : public BPFBasePolicy {
 public:
  TtsProcessPolicy();
  ~TtsProcessPolicy() override;

  sandbox::bpf_dsl::ResultExpr EvaluateSyscall(int sysno) const override;

 private:
  DISALLOW_COPY_AND_ASSIGN(TtsProcessPolicy);
};

}  // namespace service_manager

#endif  // SERVICES_SERVICE_MANAGER_SANDBOX_LINUX_BPF_TTS_POLICY_LINUX_H_
