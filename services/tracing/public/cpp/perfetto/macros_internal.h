// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_TRACING_PUBLIC_CPP_PERFETTO_MACROS_INTERNAL_H_
#define SERVICES_TRACING_PUBLIC_CPP_PERFETTO_MACROS_INTERNAL_H_

#include "build/build_config.h"
#include "third_party/perfetto/include/perfetto/tracing/event_context.h"
#include "third_party/perfetto/include/perfetto/tracing/track.h"

// Copy of function with the same name from Perfetto client library.
template <typename T>
static constexpr bool IsValidTraceLambdaImpl(
    typename std::enable_if<static_cast<bool>(
        sizeof(std::declval<T>()(std::declval<perfetto::EventContext>()),
               0))>::type* = nullptr) {
  return true;
}

template <typename T>
static constexpr bool IsValidTraceLambdaImpl(...) {
  return false;
}

template <typename T>
static constexpr bool IsValidTraceLambda() {
  return IsValidTraceLambdaImpl<T>(nullptr);
}

#if !defined(OS_IOS) && !defined(OS_NACL)

#include "base/component_export.h"
#include "base/trace_event/trace_event.h"
#include "services/tracing/public/cpp/perfetto/trace_event_data_source.h"

namespace tracing {
namespace internal {
base::Optional<base::trace_event::TraceEvent> COMPONENT_EXPORT(TRACING_CPP)
    CreateTraceEvent(char phase,
                     const unsigned char* category_group_enabled,
                     const char* name,
                     unsigned int flags,
                     base::TimeTicks timestamp);

template <
    typename TrackEventArgumentFunction = void (*)(perfetto::EventContext),
    typename ArgumentFunctionCheck = typename std::enable_if<
        IsValidTraceLambda<TrackEventArgumentFunction>()>::type>
static inline base::trace_event::TraceEventHandle AddTraceEvent(
    char phase,
    const unsigned char* category_group_enabled,
    const char* name,
    unsigned int flags,
    const perfetto::Track& track,
    base::TimeTicks timestamp,
    TrackEventArgumentFunction argument_func) {
  base::trace_event::TraceEventHandle handle = {0, 0, 0};
  auto maybe_event =
      CreateTraceEvent(phase, category_group_enabled, name, flags, timestamp);
  if (!maybe_event) {
    return handle;
  }
  TraceEventDataSource::OnAddTraceEvent(&maybe_event.value(),
                                        /* thread_will_flush = */ false,
                                        nullptr, track,
                                        std::move(argument_func));
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
      /* The parameter is an implementation detail. It allows the         */  \
      /* anonymous struct to use aggregate initialization to invoke the   */  \
      /* lambda to emit the begin event with the proper reference capture */  \
      /* for any TrackEventArgumentFunction in |__VA_ARGS__|. This is     */  \
      /* required so that the scoped event is exactly ONE line and can't  */  \
      /* escape the scope if used in a single line if statement.          */  \
      ScopedTraceEvent(...) {}                                                \
      ~ScopedTraceEvent() {                                                   \
        /* TODO(nuskos): Remove the empty string passed as the |name|  */     \
        /* field. As described in macros.h we shouldn't need it in our */     \
        /* end state.                                                  */     \
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

#else  // !defined(OS_IOS) && !defined(OS_NACL)

// Tracing isn't supported on IOS or NACL so we just black hole all the
// parameters into a function that doesn't do anything. This ensures that no
// warnings about unused parameters are generated.

namespace tracing {
namespace internal {
template <
    typename TrackEventArgumentFunction = void (*)(perfetto::EventContext)>
static inline base::trace_event::TraceEventHandle AddTraceEvent(
    char,
    const unsigned char*,
    const char*,
    unsigned int,
    const perfetto::Track&,
    base::TimeTicks,
    TrackEventArgumentFunction) {
  return {0, 0, 0};
}

}  // namespace internal
}  // namespace tracing

#define TRACING_INTERNAL_ADD_TRACE_EVENT(phase, category, name, flags, ...) \
  tracing::internal::AddTraceEvent(phase, nullptr, name, flags, ##__VA_ARGS__);

#define TRACING_INTERNAL_SCOPED_ADD_TRACE_EVENT(category, name, ...)           \
  TRACING_INTERNAL_ADD_TRACE_EVENT('B', category, name, TRACE_EVENT_FLAG_NONE, \
                                   ##__VA_ARGS__);
#endif  // else of !defined(OS_IOS) && !defined(OS_NACL)

namespace tracing {
namespace internal {

template <
    typename TrackEventArgumentFunction = void (*)(perfetto::EventContext),
    typename ArgumentFunctionCheck = typename std::enable_if<
        IsValidTraceLambda<TrackEventArgumentFunction>()>::type,
    typename TrackType,
    typename TrackTypeCheck = typename std::enable_if<
        std::is_convertible<TrackType, perfetto::Track>::value>::type>
static inline base::trace_event::TraceEventHandle AddTraceEvent(
    char phase,
    const unsigned char* category_group_enabled,
    const char* name,
    unsigned int flags,
    const TrackType& track,
    TrackEventArgumentFunction argument_func) {
  return AddTraceEvent(phase, category_group_enabled, name, flags, track,
                       base::TimeTicks(), argument_func);
}

template <
    typename TrackEventArgumentFunction = void (*)(perfetto::EventContext),
    typename ArgumentFunctionCheck = typename std::enable_if<
        IsValidTraceLambda<TrackEventArgumentFunction>()>::type>
static inline base::trace_event::TraceEventHandle AddTraceEvent(
    char phase,
    const unsigned char* category_group_enabled,
    const char* name,
    unsigned int flags,
    TrackEventArgumentFunction argument_func) {
  return AddTraceEvent(phase, category_group_enabled, name, flags,
                       perfetto::Track(), base::TimeTicks(), argument_func);
}

template <typename TrackType,
          typename TrackTypeCheck = typename std::enable_if<
              std::is_convertible<TrackType, perfetto::Track>::value>::type>
inline base::trace_event::TraceEventHandle AddTraceEvent(
    char phase,
    const unsigned char* category_group_enabled,
    const char* name,
    unsigned int flags,
    const TrackType& track) {
  return AddTraceEvent(phase, category_group_enabled, name, flags, track,
                       base::TimeTicks(), [](perfetto::EventContext ctx) {});
}

template <typename TrackType,
          typename TrackTypeCheck = typename std::enable_if<
              std::is_convertible<TrackType, perfetto::Track>::value>::type>
inline base::trace_event::TraceEventHandle AddTraceEvent(
    char phase,
    const unsigned char* category_group_enabled,
    const char* name,
    unsigned int flags,
    const TrackType& track,
    base::TimeTicks timestamp) {
  return AddTraceEvent(phase, category_group_enabled, name, flags, track,
                       timestamp, [](perfetto::EventContext ctx) {});
}

}  // namespace internal
}  // namespace tracing

#endif  // SERVICES_TRACING_PUBLIC_CPP_PERFETTO_MACROS_INTERNAL_H_
