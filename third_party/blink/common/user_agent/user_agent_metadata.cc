// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/user_agent/user_agent_metadata.h"

namespace blink {

bool operator==(const UserAgentMetadata& a, const UserAgentMetadata& b) {
  return a.brand == b.brand && a.full_version == b.full_version &&
         a.major_version == b.major_version && a.platform == b.platform &&
         a.platform_version == b.platform_version &&
         a.architecture == b.architecture && a.model == b.model &&
         a.mobile == b.mobile;
}

bool operator==(const UserAgentOverride& a, const UserAgentOverride& b) {
  return a.ua_string_override == b.ua_string_override &&
         a.ua_metadata_override == b.ua_metadata_override;
}

}  // namespace blink
