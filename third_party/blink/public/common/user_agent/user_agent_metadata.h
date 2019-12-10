// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_COMMON_USER_AGENT_USER_AGENT_METADATA_H_
#define THIRD_PARTY_BLINK_PUBLIC_COMMON_USER_AGENT_USER_AGENT_METADATA_H_

#include <string>

#include "third_party/blink/public/common/common_export.h"

namespace blink {

struct BLINK_COMMON_EXPORT UserAgentMetadata {
  std::string brand;
  std::string full_version;
  std::string major_version;
  std::string platform;
  std::string architecture;
  std::string model;
  bool mobile = false;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_COMMON_USER_AGENT_USER_AGENT_METADATA_H_
