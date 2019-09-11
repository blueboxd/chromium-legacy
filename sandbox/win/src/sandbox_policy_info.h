// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_WIN_SRC_SANDBOX_POLICY_INFO_H_
#define SANDBOX_WIN_SRC_SANDBOX_POLICY_INFO_H_

#include <vector>

#include "base/values.h"

namespace sandbox {

class PolicyBase;

// Intended to rhyme with TargetPolicy, may eventually share a common base
// with a configuration holding class (i.e. this class will extend with dynamic
// members such as the |process_ids_| list.)
class PolicyInfo {
 public:
  // This should quickly copy what it needs from PolicyBase.
  PolicyInfo(PolicyBase* policy);
  ~PolicyInfo();

  base::Value GetValue();

 private:
  std::vector<uint32_t> process_ids_;

  DISALLOW_COPY_AND_ASSIGN(PolicyInfo);
};

}  // namespace sandbox

#endif  // SANDBOX_WIN_SRC_SANDBOX_POLICY_INFO_H_
