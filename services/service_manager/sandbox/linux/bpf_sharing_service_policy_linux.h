// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_SERVICE_MANAGER_SANDBOX_LINUX_BPF_SHARING_SERVICE_POLICY_LINUX_H_
#define SERVICES_SERVICE_MANAGER_SANDBOX_LINUX_BPF_SHARING_SERVICE_POLICY_LINUX_H_

#include "base/macros.h"
#include "services/service_manager/sandbox/linux/bpf_base_policy_linux.h"

namespace service_manager {

// This policy can be used by the Sharing service to host WebRTC.
class SharingServiceProcessPolicy : public BPFBasePolicy {
 public:
  SharingServiceProcessPolicy() = default;
  ~SharingServiceProcessPolicy() override = default;

  sandbox::bpf_dsl::ResultExpr EvaluateSyscall(
      int system_call_number) const override;

  SharingServiceProcessPolicy(const SharingServiceProcessPolicy&) = delete;
  SharingServiceProcessPolicy& operator=(const SharingServiceProcessPolicy&) =
      delete;
};

}  // namespace service_manager

#endif  // SERVICES_SERVICE_MANAGER_SANDBOX_LINUX_BPF_UTILITY_POLICY_LINUX_H_
