// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_TRACING_PUBLIC_CPP_PERFETTO_MACROS_INTERNAL_H_
#define SERVICES_TRACING_PUBLIC_CPP_PERFETTO_MACROS_INTERNAL_H_

#include "base/component_export.h"
#include "base/trace_event/trace_event.h"
#include "services/tracing/public/cpp/perfetto/trace_event_data_source.h"

namespace perfetto {
class TrackEventContext;
}

namespace tracing {
namespace internal {
base::Optional<base::trace_event::TraceEvent> COMPONENT_EXPORT(TRACING_CPP)
    CreateTraceEvent(char phase,
                     const unsigned char* category_group_enabled,
                     const char* name,
                     unsigned int flags);

// A simple function that will add the TraceEvent requested and will call the
// |argument_func| after the track_event has been filled in.
template <
    typename TrackEventArgumentFunction = void (*)(perfetto::TrackEventContext)>
static inline base::trace_event::TraceEventHandle AddTraceEvent(
    char phase,
    const unsigned char* category_group_enabled,
    const char* name,
    unsigned int flags,
    TrackEventArgumentFunction argument_func) {
  base::trace_event::TraceEventHandle handle = {0, 0, 0};
  auto maybe_event =
      CreateTraceEvent(phase, category_group_enabled, name, flags);
  if (!maybe_event) {
    return handle;
  }
  TraceEventDataSource::OnAddTraceEvent(&maybe_event.value(),
                                        /* thread_will_flush = */ false,
                                        nullptr, std::move(argument_func));
  return handle;
}
}  // namespace internal
}  // namespace tracing

// These macros should not be called directly. They are intended to be used by
// macros in //services/tracing/perfetto/macros.h only.

#define TRACING_INTERNAL_CONCAT2(a, b) a##b
#define TRACING_INTERNAL_CONCAT(a, b) TRACING_INTERNAL_CONCAT2(a, b)
#define TRACING_INTERNAL_UID(prefix) TRACING_INTERNAL_CONCAT(prefix, __LINE__)

#define TRACING_INTERNAL_ADD_TRACE_EVENT(phase, category, name, flags, ...) \
  do {                                                                      \
    INTERNAL_TRACE_EVENT_GET_CATEGORY_INFO(category);                       \
    if (INTERNAL_TRACE_EVENT_CATEGORY_GROUP_ENABLED()) {                    \
      tracing::internal::AddTraceEvent(                                     \
          phase, INTERNAL_TRACE_EVENT_UID(category_group_enabled), name,    \
          flags, ##__VA_ARGS__);                                            \
    }                                                                       \
  } while (false)

#define TRACING_INTERNAL_SCOPED_ADD_TRACE_EVENT(category, name, ...)          \
  struct {                                                                    \
    struct ScopedTraceEvent {                                                 \
      /* The int parameter is an implementation detail. It allows the     */  \
      /* anonymous struct to use aggregate initialization to invoke the   */  \
      /* lambda to emit the begin event with the proper reference capture */  \
      /* for any TrackEventArgumentFunction in |__VA_ARGS__|. This is     */  \
      /* required so that the scoped event is exactly ONE line and can't  */  \
      /* escape the scope if used in a single line if statement.          */  \
      ScopedTraceEvent(int) {}                                                \
      ~ScopedTraceEvent() {                                                   \
        /* TODO(nuskos): Remove the empty string passed as the |name|  */     \
        /* field. As described in macros.h we shouldn't need it in our */     \
        /* end state                                                   */     \
        TRACING_INTERNAL_ADD_TRACE_EVENT(TRACE_EVENT_PHASE_END, category, "", \
                                         TRACE_EVENT_FLAG_NONE,               \
                                         [](perfetto::EventContext) {});      \
      }                                                                       \
    } event;                                                                  \
  } TRACING_INTERNAL_UID(scoped_event){[&]() {                                \
    TRACING_INTERNAL_ADD_TRACE_EVENT(TRACE_EVENT_PHASE_BEGIN, category, name, \
                                     TRACE_EVENT_FLAG_NONE, ##__VA_ARGS__);   \
    return 0;                                                                 \
  }()};

#endif  // SERVICES_TRACING_PUBLIC_CPP_PERFETTO_MACROS_INTERNAL_H_
