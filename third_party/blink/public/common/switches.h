// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_COMMON_SWITCHES_H_
#define THIRD_PARTY_BLINK_PUBLIC_COMMON_SWITCHES_H_

#include "third_party/blink/public/common/common_export.h"

namespace blink {
namespace switches {

// base::Feature should be use instead of switches where possible.

// All switches in alphabetical order. The switches should be documented
// alongside the definition of their values in the .cc file.
BLINK_COMMON_EXPORT extern const char kAllowPreCommitInput[];
BLINK_COMMON_EXPORT extern const char kIntensiveWakeUpThrottlingPolicy[];
BLINK_COMMON_EXPORT extern const char
    kIntensiveWakeUpThrottlingPolicy_ForceDisable[];
BLINK_COMMON_EXPORT extern const char
    kIntensiveWakeUpThrottlingPolicy_ForceEnable[];

}  // namespace switches
}  // namespace blink

#endif  // CONTENT_PUBLIC_COMMON_CONTENT_SWITCHES_H_
