// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/switches.h"

namespace blink {
namespace switches {

// Allows processing of input before a frame has been committed.
// TODO(schenney): crbug.com/987626. Used by headless. Look for a way not
// involving a command line switch.
const char kAllowPreCommitInput[] = "allow-pre-commit-input";

// Used to communicate managed policy for the IntensiveWakeUpThrottling feature.
// This feature is typically controlled by base::Feature (see
// renderer/platform/scheduler/common/features.*) but requires an enterprise
// policy override. This is implicitly a tri-state, and can be either unset, or
// set to "1" for force enable, or "0" for force disable.
extern const char kIntensiveWakeUpThrottlingPolicy[] =
    "intensive-wake-up-throttling-policy";
extern const char kIntensiveWakeUpThrottlingPolicy_ForceDisable[] = "0";
extern const char kIntensiveWakeUpThrottlingPolicy_ForceEnable[] = "1";

}  // namespace switches
}  // namespace blink
