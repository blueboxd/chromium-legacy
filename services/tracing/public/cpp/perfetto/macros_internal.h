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
                     const char* name);

// A simple function that will add the TraceEvent requested and will call the
// |argument_func| after the track_event has been filled in.
template <
    typename TrackEventArgumentFunction = void (*)(perfetto::TrackEventContext)>
static inline base::trace_event::TraceEventHandle AddTraceEvent(
    char phase,
    const unsigned char* category_group_enabled,
    const char* name,
    TrackEventArgumentFunction argument_func) {
  base::trace_event::TraceEventHandle handle = {0, 0, 0};
  auto maybe_event = CreateTraceEvent(phase, category_group_enabled, name);
  if (!maybe_event) {
    return handle;
  }
  TraceEventDataSource::OnAddTraceEvent(&maybe_event.value(),
                                        /* thread_will_flush = */ false,
                                        &handle, std::move(argument_func));
  return handle;
}
}  // namespace internal
}  // namespace tracing

// Should not be called directly. Used by macros in
// //services/tracing/perfetto/macros.h.
#define TRACING_INTERNAL_ADD_TRACE_EVENT(phase, category, name, ...)     \
  do {                                                                   \
    INTERNAL_TRACE_EVENT_GET_CATEGORY_INFO(category);                    \
    if (INTERNAL_TRACE_EVENT_CATEGORY_GROUP_ENABLED()) {                 \
      tracing::internal::AddTraceEvent(                                  \
          phase, INTERNAL_TRACE_EVENT_UID(category_group_enabled), name, \
          ##__VA_ARGS__);                                                \
    }                                                                    \
  } while (false)

#endif  // SERVICES_TRACING_PUBLIC_CPP_PERFETTO_MACROS_INTERNAL_H_
