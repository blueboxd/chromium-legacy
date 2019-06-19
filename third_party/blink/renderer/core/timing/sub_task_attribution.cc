// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/timing/sub_task_attribution.h"

namespace blink {

SubTaskAttribution::SubTaskAttribution(const AtomicString& sub_task_name,
                                       const String& script_url,
                                       base::TimeTicks start_time,
                                       base::TimeDelta duration)
    : sub_task_name_(sub_task_name),
      script_url_(script_url),
      start_time_(start_time),
      duration_(duration) {}

}  // namespace blink
