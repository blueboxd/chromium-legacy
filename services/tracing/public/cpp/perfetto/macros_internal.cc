// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/tracing/public/cpp/perfetto/macros_internal.h"

#include "base/trace_event/thread_instruction_count.h"

namespace tracing {
namespace internal {
namespace {

base::ThreadTicks ThreadNow() {
  return base::ThreadTicks::IsSupported()
             ? base::subtle::ThreadTicksNowIgnoringOverride()
             : base::ThreadTicks();
}

base::trace_event::ThreadInstructionCount ThreadInstructionNow() {
  return base::trace_event::ThreadInstructionCount::IsSupported()
             ? base::trace_event::ThreadInstructionCount::Now()
             : base::trace_event::ThreadInstructionCount();
}

}  // namespace

base::Optional<base::trace_event::TraceEvent> CreateTraceEvent(
    char phase,
    const unsigned char* category_group_enabled,
    const char* name,
    unsigned int flags) {
  DCHECK(phase == TRACE_EVENT_PHASE_BEGIN || phase == TRACE_EVENT_PHASE_END ||
         phase == TRACE_EVENT_PHASE_INSTANT);
  DCHECK(category_group_enabled);
  const int thread_id = static_cast<int>(base::PlatformThread::CurrentId());
  auto* trace_log = base::trace_event::TraceLog::GetInstance();
  DCHECK(trace_log);
  if (!trace_log->ShouldAddAfterUpdatingState(phase, category_group_enabled,
                                              name, trace_event_internal::kNoId,
                                              thread_id, nullptr)) {
    return base::nullopt;
  }

  base::TimeTicks now = TRACE_TIME_TICKS_NOW();
  base::TimeTicks offset_event_timestamp = now - base::TimeDelta();
  base::ThreadTicks thread_now = ThreadNow();
  base::trace_event::ThreadInstructionCount thread_instruction_now =
      ThreadInstructionNow();

  return base::trace_event::TraceEvent(
      thread_id, offset_event_timestamp, thread_now, thread_instruction_now,
      phase, category_group_enabled, name, trace_event_internal::kGlobalScope,
      trace_event_internal::kNoId, trace_event_internal::kNoId, nullptr, flags);
}

}  // namespace internal
}  // namespace tracing
