// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_SYNCHRONIZATION_WAITABLE_EVENT_INC_
#define BASE_SYNCHRONIZATION_WAITABLE_EVENT_INC_

#include "base/synchronization/waitable_event.h"

#include "base/trace_event/base_tracing.h"

namespace base {

void WaitableEvent::Wait() {
  TRACE_EVENT0("base", "WaitableEvent::Wait");
  bool result = TimedWait(TimeDelta::Max());
  DCHECK(result) << "TimedWait() should never fail with infinite timeout";
}

bool WaitableEvent::TimedWait(const TimeDelta& wait_delta) {
  TRACE_EVENT1("base", "WaitableEvent::TimedWait", "wait_delta_ms",
               wait_delta.InMillisecondsF());
  bool was_signaled = WaitableEvent::TimedWaitImpl(wait_delta);
  TRACE_EVENT_WITH_FLOW0("base", "WaitableEvent::TimedWait WaitFinished", this,
                         TRACE_EVENT_FLAG_FLOW_IN);
  return was_signaled;
}

}  // namespace base

#endif  // BASE_SYNCHRONIZATION_WAITABLE_EVENT_INC_
