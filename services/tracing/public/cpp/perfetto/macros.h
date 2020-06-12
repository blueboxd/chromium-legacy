// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_TRACING_PUBLIC_CPP_PERFETTO_MACROS_H_
#define SERVICES_TRACING_PUBLIC_CPP_PERFETTO_MACROS_H_

#include "base/trace_event/trace_event.h"
#include "services/tracing/public/cpp/perfetto/macros_internal.h"

// Needed not for this file but for every user of the TRACE_EVENT macros for the
// lambda definition. So included here for convenience.
#include "third_party/perfetto/include/perfetto/tracing/event_context.h"
#include "third_party/perfetto/protos/perfetto/trace/track_event/track_event.pbzero.h"

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
// Rest of parameters can contain: a perfetto::Track object for asynchronous
// events and a lambda used to fill typed event. Should be passed in that exact
// order when both are used.
//
// When lambda is passed as an argument, it is executed synchronously.
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
#define TRACE_EVENT_END(category, ...)                              \
  TRACING_INTERNAL_ADD_TRACE_EVENT(TRACE_EVENT_PHASE_END, category, \
                                   tracing::kTraceEventEndName,     \
                                   TRACE_EVENT_FLAG_NONE, ##__VA_ARGS__)

// Begin a thread-scoped slice which gets automatically closed when going out
// of scope.
//
// BEWARE: similarly to TRACE_EVENT_BEGIN, this macro does accept a track, but
// it does not work properly and should not be used.
// TODO(b/154583431): figure out how to fix or disallow that and update the
// comment.
//
// Similarly to TRACE_EVENT_BEGIN, when lambda is passed as an argument, it is
// executed synchronously.
#define TRACE_EVENT(category, name, ...) \
  TRACING_INTERNAL_SCOPED_ADD_TRACE_EVENT(category, name, ##__VA_ARGS__)

// Emit a single event called "name" immediately, with zero duration.
#define TRACE_EVENT_INSTANT(category, name, scope, ...)                       \
  TRACING_INTERNAL_ADD_TRACE_EVENT(TRACE_EVENT_PHASE_INSTANT, category, name, \
                                   scope, ##__VA_ARGS__)

#endif  // SERVICES_TRACING_PUBLIC_CPP_PERFETTO_MACROS_H_
