// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_TRACING_PUBLIC_CPP_PERFETTO_MACROS_H_
#define SERVICES_TRACING_PUBLIC_CPP_PERFETTO_MACROS_H_

#include "base/trace_event/trace_event.h"
#include "services/tracing/public/cpp/perfetto/macros_internal.h"

#if defined(TRACE_EVENT_BEGIN)
#error "Another copy of perfetto tracing macros have been included"
#endif

// TODO(nuskos): This whole file should be removed and migrated to using the
// Perfetto client library macros. However currently chromium isn't set up to
// use the client library so in the meantime we've added support for features we
// want sooner.

namespace tracing {
// The perfetto client library does not use event names for
// TRACE_EVENT_PHASE_END. However currently Chrome expects all TraceEvents to
// have event names. So until we move over to the client library we will use
// this for all TRACE_EVENT_PHASE_END typed arguments.
//
// TODO(nuskos): remove this constant.
constexpr char kTraceEventEndName[] = "";
}  // namespace tracing

// Begin a thread-scoped slice under |category| with the title |name|. Both
// strings must be static constants. The track event is only recorded if
// |category| is enabled for a tracing session.
//
// TODO(nuskos): Give a simple example once we have a typed event that doesn't
// need interning.
//   TRACE_EVENT_BEGIN("log", "LogMessage",
//       [](perfetto::EventContext ctx) {
//           auto* event = ctx.event();
//           // Fill in some field in track_event.
//       });
#define TRACE_EVENT_BEGIN(category, name, ...)                              \
  TRACING_INTERNAL_ADD_TRACE_EVENT(TRACE_EVENT_PHASE_BEGIN, category, name, \
                                   TRACE_EVENT_FLAG_NONE, ##__VA_ARGS__)

// End a thread-scoped slice under |category|.
#define TRACE_EVENT_END(category, ...)                                        \
  TRACING_INTERNAL_ADD_TRACE_EVENT(TRACE_EVENT_PHASE_END, category,           \
                                   kTraceEventEndName, TRACE_EVENT_FLAG_NONE, \
                                   ##__VA_ARGS__)

// Begin a thread-scoped slice which gets automatically closed when going out
// of scope.
#define TRACE_EVENT(category, name, ...) \
  TRACING_INTERNAL_SCOPED_ADD_TRACE_EVENT(category, name, ##__VA_ARGS__)

// Emit a thread-scoped slice which has zero duration.
// TODO(nuskos): Add support for process-wide and global instant events when
// perfetto does.
#define TRACE_EVENT_INSTANT(category, name, ...)                              \
  TRACING_INTERNAL_ADD_TRACE_EVENT(TRACE_EVENT_PHASE_INSTANT, category, name, \
                                   TRACE_EVENT_SCOPE_THREAD, ##__VA_ARGS__)

#endif  // SERVICES_TRACING_PUBLIC_CPP_PERFETTO_MACROS_H_
