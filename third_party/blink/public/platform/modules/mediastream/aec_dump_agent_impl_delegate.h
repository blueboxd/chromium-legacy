// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_PLATFORM_MODULES_MEDIASTREAM_AEC_DUMP_AGENT_IMPL_DELEGATE_H_
#define THIRD_PARTY_BLINK_PUBLIC_PLATFORM_MODULES_MEDIASTREAM_AEC_DUMP_AGENT_IMPL_DELEGATE_H_

#include "third_party/blink/public/platform/web_common.h"

namespace blink {

// TODO(crbug.com/704136): Move this class out of the Blink exposed API
// back into AecDumpAgentImpl when all its users have been Onion Souped.
class BLINK_PLATFORM_EXPORT AecDumpAgentImplDelegate {
 public:
  virtual void OnStartDump(base::File file) = 0;
  virtual void OnStopDump() = 0;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_PLATFORM_MODULES_MEDIASTREAM_AEC_DUMP_AGENT_IMPL_DELEGATE_H_
