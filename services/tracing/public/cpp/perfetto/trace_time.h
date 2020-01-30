// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_TRACING_PUBLIC_CPP_PERFETTO_TRACE_TIME_H_
#define SERVICES_TRACING_PUBLIC_CPP_PERFETTO_TRACE_TIME_H_

#include "build/build_config.h"
#include "third_party/perfetto/protos/perfetto/trace/clock_snapshot.pbzero.h"

namespace tracing {

#if defined(OS_LINUX) || defined(OS_ANDROID) || defined(OS_FUCHSIA)
// Linux, Android, and Fuchsia all use CLOCK_MONOTONIC. See crbug.com/166153
// about efforts to unify base::TimeTicks across all platforms.
constexpr perfetto::protos::pbzero::ClockSnapshot::Clock::BuiltinClocks
    kTraceClockId = perfetto::protos::pbzero::ClockSnapshot::Clock::MONOTONIC;
#else
// Mac and Windows TimeTicks advance when sleeping, so are closest to BOOTTIME
// in behavior.
// TODO(eseckler): Support specifying Mac/Win platform clocks in BuiltinClocks.
constexpr perfetto::protos::pbzero::ClockSnapshot::Clock::BuiltinClocks
    kTraceClockId = perfetto::protos::pbzero::ClockSnapshot::Clock::BOOTTIME;
#endif

// Returns CLOCK_BOOTTIME on systems that support it, otherwise falls back to
// TRACE_TIME_TICKS_NOW().
int64_t TraceBootTicksNow();

}  // namespace tracing

#endif  // SERVICES_TRACING_PUBLIC_CPP_PERFETTO_TRACE_TIME_H_
