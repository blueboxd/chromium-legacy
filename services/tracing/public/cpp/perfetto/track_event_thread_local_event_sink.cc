// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/tracing/public/cpp/perfetto/track_event_thread_local_event_sink.h"

#include <algorithm>
#include <atomic>

#include "base/stl_util.h"
#include "base/strings/pattern.h"
#include "base/strings/strcat.h"
#include "base/trace_event/common/trace_event_common.h"
#include "base/trace_event/log_message.h"
#include "base/trace_event/trace_buffer.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/trace_log.h"
#include "base/trace_event/traced_value.h"
#include "build/build_config.h"
#include "services/tracing/public/cpp/perfetto/perfetto_traced_process.h"
#include "services/tracing/public/cpp/perfetto/producer_client.h"
#include "services/tracing/public/cpp/perfetto/trace_event_data_source.h"
#include "services/tracing/public/cpp/perfetto/trace_time.h"
#include "services/tracing/public/cpp/perfetto/traced_value_proto_writer.h"
#include "third_party/perfetto/include/perfetto/tracing/track.h"
#include "third_party/perfetto/protos/perfetto/trace/clock_snapshot.pbzero.h"
#include "third_party/perfetto/protos/perfetto/trace/trace_packet.pbzero.h"
#include "third_party/perfetto/protos/perfetto/trace/trace_packet_defaults.pbzero.h"
#include "third_party/perfetto/protos/perfetto/trace/track_event/debug_annotation.pbzero.h"
#include "third_party/perfetto/protos/perfetto/trace/track_event/log_message.pbzero.h"
#include "third_party/perfetto/protos/perfetto/trace/track_event/process_descriptor.pbzero.h"
#include "third_party/perfetto/protos/perfetto/trace/track_event/source_location.pbzero.h"
#include "third_party/perfetto/protos/perfetto/trace/track_event/task_execution.pbzero.h"
#include "third_party/perfetto/protos/perfetto/trace/track_event/thread_descriptor.pbzero.h"
#include "third_party/perfetto/protos/perfetto/trace/track_event/track_descriptor.pbzero.h"

using TraceLog = base::trace_event::TraceLog;
using perfetto::protos::pbzero::ChromeThreadDescriptor;
using perfetto::protos::pbzero::ClockSnapshot;
using perfetto::protos::pbzero::ThreadDescriptor;
using perfetto::protos::pbzero::TracePacket;
using perfetto::protos::pbzero::TracePacketDefaults;
using perfetto::protos::pbzero::TrackDescriptor;
using perfetto::protos::pbzero::TrackEvent;
using perfetto::protos::pbzero::TrackEventDefaults;

namespace tracing {

namespace {

// Replacement string for names of events with TRACE_EVENT_FLAG_COPY.
const char* const kPrivacyFiltered = "PRIVACY_FILTERED";

// Packet-sequence-scoped clocks that encode timestamps in microseconds in
// the kTraceClockId clock domain.
constexpr uint32_t kClockIdAbsolute = 64;
constexpr uint32_t kClockIdIncremental = 65;

base::ThreadTicks ThreadNow() {
  return base::ThreadTicks::IsSupported()
             ? base::subtle::ThreadTicksNowIgnoringOverride()
             : base::ThreadTicks();
}

// Names of events that should be converted into a TaskExecution event.
const char* kTaskExecutionEventCategory = "toplevel";
const char* kTaskExecutionEventNames[3] = {"ThreadControllerImpl::RunTask",
                                           "ThreadController::Task",
                                           "ThreadPool_RunTask"};

void AddConvertableToTraceFormat(
    base::trace_event::ConvertableToTraceFormat* value,
    perfetto::protos::pbzero::DebugAnnotation* annotation) {
  PerfettoProtoAppender proto_appender(annotation);
  if (value->AppendToProto(&proto_appender)) {
    return;
  }

  std::string json;
  value->AppendAsTraceFormat(&json);
  annotation->set_legacy_json_value(json.c_str());
}

void WriteDebugAnnotations(
    base::trace_event::TraceEvent* trace_event,
    TrackEvent* track_event,
    InterningIndexEntry* current_packet_interning_entries) {
  for (size_t i = 0; i < trace_event->arg_size() && trace_event->arg_name(i);
       ++i) {
    auto type = trace_event->arg_type(i);
    auto* annotation = track_event->add_debug_annotations();

    annotation->set_name_iid(current_packet_interning_entries[i].id);

    if (type == TRACE_VALUE_TYPE_CONVERTABLE) {
      AddConvertableToTraceFormat(trace_event->arg_convertible_value(i),
                                  annotation);
      continue;
    }

    auto& value = trace_event->arg_value(i);
    switch (type) {
      case TRACE_VALUE_TYPE_BOOL:
        annotation->set_bool_value(value.as_bool);
        break;
      case TRACE_VALUE_TYPE_UINT:
        annotation->set_uint_value(value.as_uint);
        break;
      case TRACE_VALUE_TYPE_INT:
        annotation->set_int_value(value.as_int);
        break;
      case TRACE_VALUE_TYPE_DOUBLE:
        annotation->set_double_value(value.as_double);
        break;
      case TRACE_VALUE_TYPE_POINTER:
        annotation->set_pointer_value(static_cast<uint64_t>(
            reinterpret_cast<uintptr_t>(value.as_pointer)));
        break;
      case TRACE_VALUE_TYPE_STRING:
      case TRACE_VALUE_TYPE_COPY_STRING:
        annotation->set_string_value(value.as_string ? value.as_string
                                                     : "NULL");
        break;
      default:
        NOTREACHED() << "Don't know how to serialize this value";
        break;
    }
  }
}

ChromeThreadDescriptor::ThreadType GetThreadType(
    const char* const thread_name) {
  if (base::MatchPattern(thread_name, "Cr*Main")) {
    return ChromeThreadDescriptor::THREAD_MAIN;
  } else if (base::MatchPattern(thread_name, "Chrome*IOThread")) {
    return ChromeThreadDescriptor::THREAD_IO;
  } else if (base::MatchPattern(thread_name, "ThreadPoolForegroundWorker*")) {
    return ChromeThreadDescriptor::THREAD_POOL_FG_WORKER;
  } else if (base::MatchPattern(thread_name, "ThreadPoolBackgroundWorker*")) {
    return ChromeThreadDescriptor::THREAD_POOL_BG_WORKER;
  } else if (base::MatchPattern(thread_name,
                                "ThreadPool*ForegroundBlocking*")) {
    return ChromeThreadDescriptor::THREAD_POOL_FG_BLOCKING;
  } else if (base::MatchPattern(thread_name,
                                "ThreadPool*BackgroundBlocking*")) {
    return ChromeThreadDescriptor::THREAD_POOL_BG_BLOCKING;
  } else if (base::MatchPattern(thread_name, "ThreadPoolService*")) {
    return ChromeThreadDescriptor::THREAD_POOL_SERVICE;
  } else if (base::MatchPattern(thread_name, "CompositorTileWorker*")) {
    return ChromeThreadDescriptor::THREAD_COMPOSITOR_WORKER;
  } else if (base::MatchPattern(thread_name, "Compositor")) {
    return ChromeThreadDescriptor::THREAD_COMPOSITOR;
  } else if (base::MatchPattern(thread_name, "VizCompositor*")) {
    return ChromeThreadDescriptor::THREAD_VIZ_COMPOSITOR;
  } else if (base::MatchPattern(thread_name, "ServiceWorker*")) {
    return ChromeThreadDescriptor::THREAD_SERVICE_WORKER;
  } else if (base::MatchPattern(thread_name, "MemoryInfra")) {
    return ChromeThreadDescriptor::THREAD_MEMORY_INFRA;
  } else if (base::MatchPattern(thread_name, "StackSamplingProfiler")) {
    return ChromeThreadDescriptor::THREAD_SAMPLING_PROFILER;
  }
  return ChromeThreadDescriptor::THREAD_UNSPECIFIED;
}

// Lazily sets |legacy_event| on the |track_event|. Note that you should not set
// any other fields of |track_event| (outside the LegacyEvent) between any calls
// to GetOrCreate(), as the protozero serialization requires the writes to a
// message's fields to be consecutive.
class LazyLegacyEventInitializer {
 public:
  LazyLegacyEventInitializer(TrackEvent* track_event)
      : track_event_(track_event) {}

  TrackEvent::LegacyEvent* GetOrCreate() {
    if (!legacy_event_) {
      legacy_event_ = track_event_->set_legacy_event();
    }
    return legacy_event_;
  }

 private:
  TrackEvent* track_event_;
  TrackEvent::LegacyEvent* legacy_event_ = nullptr;
};

}  // namespace

// static
constexpr size_t TrackEventThreadLocalEventSink::kMaxCompleteEventDepth;

// static
std::atomic<uint32_t>
    TrackEventThreadLocalEventSink::incremental_state_reset_id_{0};

TrackEventThreadLocalEventSink::IndexData::IndexData(const char* str)
    : str_piece(str) {}

TrackEventThreadLocalEventSink::IndexData::IndexData(
    std::tuple<const char*, const char*, int>&& src)
    : src_loc(std::move(src)) {}

TrackEventThreadLocalEventSink::TrackEventThreadLocalEventSink(
    std::unique_ptr<perfetto::TraceWriter> trace_writer,
    uint32_t session_id,
    bool disable_interning,
    bool proto_writer_filtering_enabled)
    : process_id_(TraceLog::GetInstance()->process_id()),
      thread_id_(static_cast<int>(base::PlatformThread::CurrentId())),
      privacy_filtering_enabled_(proto_writer_filtering_enabled),
      trace_writer_(std::move(trace_writer)),
      session_id_(session_id),
      disable_interning_(disable_interning) {
  static std::atomic<uint32_t> g_sink_id_counter{0};
  sink_id_ = ++g_sink_id_counter;
  base::ThreadIdNameManager::GetInstance()->AddObserver(this);
}

TrackEventThreadLocalEventSink::~TrackEventThreadLocalEventSink() {
  base::ThreadIdNameManager::GetInstance()->RemoveObserver(this);

  // We've already destroyed all message handles at this point.
  TraceEventDataSource::GetInstance()->ReturnTraceWriter(
      std::move(trace_writer_));
}

// static
void TrackEventThreadLocalEventSink::ClearIncrementalState() {
  incremental_state_reset_id_.fetch_add(1u, std::memory_order_relaxed);
}

void TrackEventThreadLocalEventSink::ResetIncrementalStateIfNeeded(
    base::trace_event::TraceEvent* trace_event) {
  bool explicit_timestamp =
      trace_event->flags() & TRACE_EVENT_FLAG_EXPLICIT_TIMESTAMP;

  // We access |incremental_state_reset_id_| atomically but racily. It's OK if
  // we don't notice the reset request immediately, as long as we will notice
  // and service it eventually.
  auto reset_id = incremental_state_reset_id_.load(std::memory_order_relaxed);
  if (reset_id != last_incremental_state_reset_id_) {
    reset_incremental_state_ = true;
    last_incremental_state_reset_id_ = reset_id;
  }

  if (reset_incremental_state_) {
    DoResetIncrementalState(trace_event, explicit_timestamp);
  }
}

TrackEvent* TrackEventThreadLocalEventSink::PrepareTrackEvent(
    base::trace_event::TraceEvent* trace_event,
    base::trace_event::TraceEventHandle* handle,
    protozero::MessageHandle<TracePacket>* trace_packet) {
  // Each event's updates to InternedData are flushed at the end of
  // AddTraceEvent().
  DCHECK(pending_interning_updates_.empty());

  char phase = trace_event->phase();

  // Split COMPLETE events into BEGIN/END pairs. We write the BEGIN here, and
  // the END in UpdateDuration().
  if (phase == TRACE_EVENT_PHASE_COMPLETE) {
    phase = TRACE_EVENT_PHASE_BEGIN;
  }

  bool is_sync_end = phase == TRACE_EVENT_PHASE_END;
  bool is_nestable_async_end = phase == TRACE_EVENT_PHASE_NESTABLE_ASYNC_END;

  uint32_t flags = trace_event->flags();
  bool copy_strings = flags & TRACE_EVENT_FLAG_COPY;
  bool is_java_event = flags & TRACE_EVENT_FLAG_JAVA_STRING_LITERALS;
  bool explicit_timestamp = flags & TRACE_EVENT_FLAG_EXPLICIT_TIMESTAMP;

  // Delta encoded timestamps and interned data require incremental state.
  (*trace_packet)->set_sequence_flags(TracePacket::SEQ_NEEDS_INCREMENTAL_STATE);

  // Events for different processes/threads always use an absolute timestamp.
  bool is_for_different_process =
      (flags & TRACE_EVENT_FLAG_HAS_PROCESS_ID) &&
      trace_event->process_id() != base::kNullProcessId;
  bool is_for_different_thread = thread_id_ != trace_event->thread_id();
  bool force_absolute_timestamp =
      is_for_different_process || is_for_different_thread || explicit_timestamp;

  SetPacketTimestamp(trace_packet, trace_event->timestamp(),
                     force_absolute_timestamp);

  TrackEvent* track_event = (*trace_packet)->set_track_event();

  const char* category_name =
      TraceLog::GetCategoryGroupName(trace_event->category_group_enabled());
  InterningIndexEntry interned_category{};
  // No need to write the category for sync end events. Trace processor will
  // match them without, provided event nesting is correct. For async end
  // events, event ID matching includes the category, so we have to emit the
  // category for the time being.
  if (!is_sync_end) {
    interned_category = interned_event_categories_.LookupOrAdd(category_name);
  }

  InterningIndexEntry interned_name{};
  const size_t kMaxSize = base::trace_event::TraceArguments::kMaxSize;
  InterningIndexEntry interned_annotation_names[kMaxSize] = {
      InterningIndexEntry{}};
  InterningIndexEntry interned_source_location{};
  InterningIndexEntry interned_log_message_body{};

  const char* src_file = nullptr;
  const char* src_func = nullptr;
  const char* log_message_body = nullptr;
  int line_number = 0;

  // No need to write the event name for end events (sync or nestable async).
  // Trace processor will match them without, provided event nesting is correct.
  // Legacy async events (TRACE_EVENT_ASYNC*) are only pass-through in trace
  // processor, so we still have to emit names for these.
  const char* trace_event_name = trace_event->name();
  if (!is_sync_end && !is_nestable_async_end) {
    if (copy_strings) {
      if (!is_java_event && privacy_filtering_enabled_) {
        trace_event_name = kPrivacyFiltered;
        interned_name = interned_event_names_.LookupOrAdd(trace_event_name);
      } else {
        interned_name =
            interned_event_names_.LookupOrAdd(std::string(trace_event_name));
      }
    } else {
      interned_name = interned_event_names_.LookupOrAdd(trace_event->name());
    }
  }

  if (copy_strings) {
    if (is_java_event || !privacy_filtering_enabled_) {
      for (size_t i = 0;
           i < trace_event->arg_size() && trace_event->arg_name(i); ++i) {
        interned_annotation_names[i] = interned_annotation_names_.LookupOrAdd(
            std::string(trace_event->arg_name(i)));
      }
    }
  } else {
    // TODO(eseckler): Remove special handling of typed events here once we
    // support them in TRACE_EVENT macros.

    if (flags & TRACE_EVENT_FLAG_TYPED_PROTO_ARGS) {
      if (trace_event->arg_size() == 2u) {
        DCHECK_EQ(strcmp(category_name, kTaskExecutionEventCategory), 0);
        DCHECK(strcmp(trace_event->name(), kTaskExecutionEventNames[0]) == 0 ||
               strcmp(trace_event->name(), kTaskExecutionEventNames[1]) == 0 ||
               strcmp(trace_event->name(), kTaskExecutionEventNames[2]) == 0);
        // Double argument task execution event (src_file, src_func).
        DCHECK_EQ(trace_event->arg_type(0), TRACE_VALUE_TYPE_STRING);
        DCHECK_EQ(trace_event->arg_type(1), TRACE_VALUE_TYPE_STRING);
        src_file = trace_event->arg_value(0).as_string;
        src_func = trace_event->arg_value(1).as_string;
      } else {
        // arg_size == 1 enforced by the maximum number of parameter == 2.
        DCHECK_EQ(trace_event->arg_size(), 1u);

        if (trace_event->arg_type(0) == TRACE_VALUE_TYPE_STRING) {
          // Single argument task execution event (src_file).
          DCHECK_EQ(strcmp(category_name, kTaskExecutionEventCategory), 0);
          DCHECK(
              strcmp(trace_event->name(), kTaskExecutionEventNames[0]) == 0 ||
              strcmp(trace_event->name(), kTaskExecutionEventNames[1]) == 0 ||
              strcmp(trace_event->name(), kTaskExecutionEventNames[2]) == 0);
          src_file = trace_event->arg_value(0).as_string;
        } else {
          DCHECK_EQ(trace_event->arg_type(0), TRACE_VALUE_TYPE_CONVERTABLE);
          DCHECK(strcmp(category_name, "log") == 0);
          DCHECK(strcmp(trace_event->name(), "LogMessage") == 0);
          const base::trace_event::LogMessage* value =
              static_cast<base::trace_event::LogMessage*>(
                  trace_event->arg_value(0).as_convertable);
          src_file = value->file();
          line_number = value->line_number();
          log_message_body = value->message().c_str();

          interned_log_message_body =
              interned_log_message_bodies_.LookupOrAdd(value->message());
        }  // else
      }    // else
      interned_source_location = interned_source_locations_.LookupOrAdd(
          std::make_tuple(src_file, src_func, line_number));
    } else if (!privacy_filtering_enabled_) {
      for (size_t i = 0;
           i < trace_event->arg_size() && trace_event->arg_name(i); ++i) {
        interned_annotation_names[i] =
            interned_annotation_names_.LookupOrAdd(trace_event->arg_name(i));
      }
    }
  }

  if (!trace_event->thread_timestamp().is_null()) {
    // Thread timestamps are never user-provided, but COMPLETE events may get
    // reordered, so we can still observe timestamps that are further in the
    // past. Emit those as absolute timestamps, since we don't support
    // negative deltas.
    if (last_thread_time_ > trace_event->thread_timestamp()) {
      track_event->set_thread_time_absolute_us(
          trace_event->thread_timestamp().since_origin().InMicroseconds());
    } else {
      track_event->set_thread_time_delta_us(
          (trace_event->thread_timestamp() - last_thread_time_)
              .InMicroseconds());
      last_thread_time_ = trace_event->thread_timestamp();
    }
  }

  if (!trace_event->thread_instruction_count().is_null()) {
    // Thread instruction counts are never user-provided, but COMPLETE events
    // may get reordered, so we can still observe counts that are lower. Emit
    // those as absolute counts, since we don't support negative deltas.
    if (last_thread_instruction_count_.ToInternalValue() >
        trace_event->thread_instruction_count().ToInternalValue()) {
      track_event->set_thread_instruction_count_absolute(
          trace_event->thread_instruction_count().ToInternalValue());
    } else {
      track_event->set_thread_instruction_count_delta(
          (trace_event->thread_instruction_count() -
           last_thread_instruction_count_)
              .ToInternalValue());
      last_thread_instruction_count_ = trace_event->thread_instruction_count();
    }
  }

  // TODO(eseckler): Split comma-separated category strings.
  if (interned_category.id) {
    track_event->add_category_iids(interned_category.id);
  }

  if (interned_log_message_body.id) {
    auto* log_message = track_event->set_log_message();
    log_message->set_source_location_iid(interned_source_location.id);
    log_message->set_body_iid(interned_log_message_body.id);
  } else if (interned_source_location.id) {
    track_event->set_task_execution()->set_posted_from_iid(
        interned_source_location.id);
  } else if (!privacy_filtering_enabled_) {
    WriteDebugAnnotations(trace_event, track_event, interned_annotation_names);
  }

  if (interned_name.id) {
    track_event->set_name_iid(interned_name.id);
  }

  // Only set the |legacy_event| field of the TrackEvent if we need to emit any
  // of the legacy fields. BEWARE: Do not set any other TrackEvent fields in
  // between calls to |legacy_event.GetOrCreate()|.
  LazyLegacyEventInitializer legacy_event(track_event);

  // TODO(eseckler): Also convert async / flow events to corresponding native
  // TrackEvent types. Instants & asyncs require using track descriptors rather
  // than instant event scopes / async event IDs.
  TrackEvent::Type track_event_type = TrackEvent::TYPE_UNSPECIFIED;
  switch (phase) {
    case TRACE_EVENT_PHASE_BEGIN:
      track_event_type = TrackEvent::TYPE_SLICE_BEGIN;
      break;
    case TRACE_EVENT_PHASE_END:
      track_event_type = TrackEvent::TYPE_SLICE_END;
      break;
    case TRACE_EVENT_PHASE_INSTANT:
      track_event_type = TrackEvent::TYPE_INSTANT;
      break;
    default:
      break;
  }

  if (track_event_type != TrackEvent::TYPE_UNSPECIFIED) {
    // We emit these events using TrackDescriptors, and we cannot emit events on
    // behalf of other processes using the TrackDescriptor format. Chrome
    // currently only emits PHASE_MEMORY_DUMP events with an explicit process
    // id, so we should be fine here.
    // TODO(eseckler): Get rid of events with explicit process ids entirely.
    DCHECK(!(flags & TRACE_EVENT_FLAG_HAS_PROCESS_ID));

    track_event->set_type(track_event_type);

    if (track_event_type == TrackEvent::TYPE_INSTANT) {
      switch (flags & TRACE_EVENT_FLAG_SCOPE_MASK) {
        case TRACE_EVENT_SCOPE_GLOBAL: {
          track_event->set_track_uuid(0);  // Global track.
          break;
        }
        case TRACE_EVENT_SCOPE_PROCESS: {
          track_event->set_track_uuid(perfetto::ProcessTrack::Current().uuid);
          break;
        }
        case TRACE_EVENT_SCOPE_THREAD: {
          if (thread_id_ != trace_event->thread_id()) {
            track_event->set_track_uuid(
                perfetto::ThreadTrack::ForThread(trace_event->thread_id())
                    .uuid);
          } else {
            // Default to the thread track.
          }
          break;
        }
      }
    } else {
      if (thread_id_ != trace_event->thread_id()) {
        track_event->set_track_uuid(
            perfetto::ThreadTrack::ForThread(trace_event->thread_id()).uuid);
      } else {
        // Default to the thread track.
      }
    }
  } else {
    // Explicitly clear the track, so that the event is not associated with the
    // default track, but instead uses the legacy mechanism based on the phase
    // and pid/tid override.
    track_event->set_track_uuid(0);

    legacy_event.GetOrCreate()->set_phase(phase);

    if ((flags & TRACE_EVENT_FLAG_HAS_PROCESS_ID) &&
        trace_event->process_id() != base::kNullProcessId) {
      legacy_event.GetOrCreate()->set_pid_override(trace_event->process_id());
      legacy_event.GetOrCreate()->set_tid_override(-1);
    } else if (thread_id_ != trace_event->thread_id()) {
      legacy_event.GetOrCreate()->set_tid_override(trace_event->thread_id());
    }
  }

  uint32_t id_flags =
      flags & (TRACE_EVENT_FLAG_HAS_ID | TRACE_EVENT_FLAG_HAS_LOCAL_ID |
               TRACE_EVENT_FLAG_HAS_GLOBAL_ID);
  uint32_t flow_flags =
      flags & (TRACE_EVENT_FLAG_FLOW_OUT | TRACE_EVENT_FLAG_FLOW_IN);

  // Legacy flow events use bind_id as their (unscoped) identifier. There's no
  // need to also emit id in that case.
  if (!flow_flags) {
    switch (id_flags) {
      case TRACE_EVENT_FLAG_HAS_ID:
        legacy_event.GetOrCreate()->set_unscoped_id(trace_event->id());
        break;
      case TRACE_EVENT_FLAG_HAS_LOCAL_ID:
        legacy_event.GetOrCreate()->set_local_id(trace_event->id());
        break;
      case TRACE_EVENT_FLAG_HAS_GLOBAL_ID:
        legacy_event.GetOrCreate()->set_global_id(trace_event->id());
        break;
      default:
        break;
    }
  }

  // TODO(ssid): Add scope field as enum and do not filter this field.
  if (!privacy_filtering_enabled_) {
    if (id_flags &&
        trace_event->scope() != trace_event_internal::kGlobalScope) {
      legacy_event.GetOrCreate()->set_id_scope(trace_event->scope());
    }
  }

  if (flags & TRACE_EVENT_FLAG_ASYNC_TTS) {
    legacy_event.GetOrCreate()->set_use_async_tts(true);
  }

  switch (flow_flags) {
    case TRACE_EVENT_FLAG_FLOW_OUT | TRACE_EVENT_FLAG_FLOW_IN:
      legacy_event.GetOrCreate()->set_flow_direction(
          TrackEvent::LegacyEvent::FLOW_INOUT);
      break;
    case TRACE_EVENT_FLAG_FLOW_OUT:
      legacy_event.GetOrCreate()->set_flow_direction(
          TrackEvent::LegacyEvent::FLOW_OUT);
      break;
    case TRACE_EVENT_FLAG_FLOW_IN:
      legacy_event.GetOrCreate()->set_flow_direction(
          TrackEvent::LegacyEvent::FLOW_IN);
      break;
    default:
      break;
  }

  if (flow_flags) {
    legacy_event.GetOrCreate()->set_bind_id(trace_event->bind_id());
  }

  if (flags & TRACE_EVENT_FLAG_BIND_TO_ENCLOSING) {
    legacy_event.GetOrCreate()->set_bind_to_enclosing(true);
  }

  if (interned_category.id && !interned_category.was_emitted) {
    pending_interning_updates_.push_back(
        std::make_tuple(IndexType::kCategory, IndexData{category_name},
                        std::move(interned_category)));
  }
  if (interned_name.id && !interned_name.was_emitted) {
    pending_interning_updates_.push_back(
        std::make_tuple(IndexType::kName, IndexData{trace_event_name},
                        std::move(interned_name)));
  }
  if (interned_log_message_body.id && !interned_log_message_body.was_emitted) {
    pending_interning_updates_.push_back(
        std::make_tuple(IndexType::kLogMessage, IndexData{log_message_body},
                        std::move(interned_log_message_body)));
  }
  if (interned_source_location.id) {
    if (!interned_source_location.was_emitted) {
      pending_interning_updates_.push_back(std::make_tuple(
          IndexType::kSourceLocation,
          IndexData{std::make_tuple(src_file, src_func, line_number)},
          std::move(interned_source_location)));
    }
  } else if (!privacy_filtering_enabled_) {
    for (size_t i = 0; i < trace_event->arg_size() && trace_event->arg_name(i);
         ++i) {
      DCHECK(interned_annotation_names[i].id);
      if (!interned_annotation_names[i].was_emitted) {
        pending_interning_updates_.push_back(std::make_tuple(
            IndexType::kAnnotationName, IndexData{trace_event->arg_name(i)},
            std::move(interned_annotation_names[i])));
      }
    }
  }
  if (disable_interning_) {
    interned_event_categories_.Clear();
    interned_event_names_.Clear();
    interned_annotation_names_.Clear();
    interned_source_locations_.Clear();
    interned_log_message_bodies_.Clear();
  }

  return track_event;
}

void TrackEventThreadLocalEventSink::EmitStoredInternedData(
    perfetto::protos::pbzero::InternedData* interned_data) {
  DCHECK(interned_data);
  for (const auto& update : pending_interning_updates_) {
    IndexType type = std::get<0>(update);
    IndexData data = std::get<1>(update);
    InterningIndexEntry entry = std::get<2>(update);
    if (type == IndexType::kName) {
      auto* name_entry = interned_data->add_event_names();
      name_entry->set_iid(entry.id);
      name_entry->set_name(data.str_piece);
    } else if (type == IndexType::kCategory) {
      auto* category_entry = interned_data->add_event_categories();
      category_entry->set_iid(entry.id);
      category_entry->set_name(data.str_piece);
    } else if (type == IndexType::kLogMessage) {
      auto* log_message_entry = interned_data->add_log_message_body();
      log_message_entry->set_iid(entry.id);
      log_message_entry->set_body(data.str_piece);
    } else if (type == IndexType::kSourceLocation) {
      auto* source_location_entry = interned_data->add_source_locations();
      source_location_entry->set_iid(entry.id);
      source_location_entry->set_file_name(std::get<0>(data.src_loc));

      if (std::get<1>(data.src_loc)) {
        source_location_entry->set_function_name(std::get<1>(data.src_loc));
      }
      if (std::get<2>(data.src_loc)) {
        source_location_entry->set_line_number(std::get<2>(data.src_loc));
      }
    } else if (type == IndexType::kAnnotationName) {
      DCHECK(!privacy_filtering_enabled_);
      auto* name_entry = interned_data->add_debug_annotation_names();
      name_entry->set_iid(entry.id);
      name_entry->set_name(data.str_piece);
    } else {
      DLOG(FATAL) << "Unhandled type: " << static_cast<int>(type);
    }
  }
  pending_interning_updates_.clear();
}

void TrackEventThreadLocalEventSink::UpdateDuration(
    const unsigned char* category_group_enabled,
    const char* name,
    base::trace_event::TraceEventHandle handle,
    int thread_id,
    bool explicit_timestamps,
    const base::TimeTicks& now,
    const base::ThreadTicks& thread_now,
    base::trace_event::ThreadInstructionCount thread_instruction_now) {
  base::trace_event::TraceEvent new_trace_event(
      thread_id, now, thread_now, thread_instruction_now, TRACE_EVENT_PHASE_END,
      category_group_enabled, name, trace_event_internal::kGlobalScope,
      trace_event_internal::kNoId /* id */,
      trace_event_internal::kNoId /* bind_id */, nullptr,
      explicit_timestamps ? TRACE_EVENT_FLAG_EXPLICIT_TIMESTAMP
                          : TRACE_EVENT_FLAG_NONE);
  AddTraceEvent(&new_trace_event, nullptr, [](perfetto::EventContext) {});
}

void TrackEventThreadLocalEventSink::Flush() {
  trace_writer_->Flush();
}

void TrackEventThreadLocalEventSink::OnThreadNameChanged(const char* name) {
  if (thread_id_ != static_cast<int>(base::PlatformThread::CurrentId()))
    return;
  auto trace_packet = trace_writer_->NewTracePacket();
  EmitTrackDescriptor(&trace_packet, nullptr, TRACE_TIME_TICKS_NOW(), name);
}

void TrackEventThreadLocalEventSink::EmitTrackDescriptor(
    protozero::MessageHandle<TracePacket>* trace_packet,
    base::trace_event::TraceEvent* trace_event,
    base::TimeTicks timestamp,
    const char* maybe_new_name) {
  SetPacketTimestamp(trace_packet, timestamp);

  TrackDescriptor* track_descriptor = (*trace_packet)->set_track_descriptor();
  // TODO(eseckler): Call ThreadTrack::Current() instead once the client lib
  // supports Chrome's tids.
  auto thread_track = perfetto::ThreadTrack::ForThread(thread_id_);

  // TODO(eseckler): Call thread_track.Serialize() here instead once the client
  // lib also fills in the ThreadDescriptor's thread_name, gets the correct pid
  // from Chrome, and supports pivacy filtering, and we moved off reference_*
  // fields in ThreadDescriptor.
  track_descriptor->set_uuid(thread_track.uuid);
  PERFETTO_DCHECK(thread_track.parent_uuid);
  track_descriptor->set_parent_uuid(thread_track.parent_uuid);

  ThreadDescriptor* thread = track_descriptor->set_thread();
  thread->set_pid(process_id_);
  thread->set_tid(thread_id_);

  if (!maybe_new_name) {
    maybe_new_name =
        base::ThreadIdNameManager::GetInstance()->GetNameForCurrentThread();
  }
  if (maybe_new_name && *maybe_new_name &&
      base::StringPiece(thread_name_) != maybe_new_name) {
    thread_name_ = maybe_new_name;
    thread_type_ = GetThreadType(maybe_new_name);
  }
  if (!privacy_filtering_enabled_) {
    thread->set_thread_name(thread_name_);
  }

  // TODO(eseckler): Switch to a more generic encoding format for counters and
  // move them out of ThreadDescriptor.
  if (!trace_event || trace_event->thread_timestamp().is_null()) {
    last_thread_time_ = ThreadNow();
  } else {
    // Thread timestamp is never user-provided.
    DCHECK_LE(trace_event->thread_timestamp(), ThreadNow());
    last_thread_time_ = trace_event->thread_timestamp();
  }
  thread->set_reference_thread_time_us(
      last_thread_time_.since_origin().InMicroseconds());

  if (base::trace_event::ThreadInstructionCount::IsSupported()) {
    if (!trace_event || trace_event->thread_instruction_count().is_null()) {
      last_thread_instruction_count_ =
          base::trace_event::ThreadInstructionCount::Now();
    } else {
      // Thread instruction count is never user-provided.
      DCHECK_LE(
          trace_event->thread_instruction_count().ToInternalValue(),
          base::trace_event::ThreadInstructionCount::Now().ToInternalValue());
      last_thread_instruction_count_ = trace_event->thread_instruction_count();
    }
    thread->set_reference_thread_instruction_count(
        last_thread_instruction_count_.ToInternalValue());
  }

  ChromeThreadDescriptor* chrome_thread = track_descriptor->set_chrome_thread();
  chrome_thread->set_thread_type(thread_type_);

  // TODO(eseckler): Fill in remaining fields in ChromeThreadDescriptor.
}

void TrackEventThreadLocalEventSink::DoResetIncrementalState(
    base::trace_event::TraceEvent* trace_event,
    bool explicit_timestamp) {
  interned_event_categories_.ResetEmittedState();
  interned_event_names_.ResetEmittedState();
  interned_annotation_names_.ResetEmittedState();
  interned_source_locations_.ResetEmittedState();
  interned_log_message_bodies_.ResetEmittedState();

  // Reset the reference timestamp.
  base::TimeTicks timestamp;
  if (explicit_timestamp || !trace_event) {
    timestamp = TRACE_TIME_TICKS_NOW();
  } else {
    timestamp = trace_event->timestamp();
  }
  last_timestamp_ = timestamp;

  {
    // Emit a new clock snapshot with this timestamp, and also set the
    // |incremental_state_cleared| flag and defaults.
    auto packet = trace_writer_->NewTracePacket();
    packet->set_sequence_flags(TracePacket::SEQ_INCREMENTAL_STATE_CLEARED);

    TracePacketDefaults* tp_defaults = packet->set_trace_packet_defaults();
    tp_defaults->set_timestamp_clock_id(kClockIdIncremental);
    TrackEventDefaults* te_defaults = tp_defaults->set_track_event_defaults();
    // TODO(eseckler): Call ThreadTrack::Current() instead once the client lib
    // supports Chrome's tids.
    te_defaults->set_track_uuid(
        perfetto::ThreadTrack::ForThread(thread_id_).uuid);

    ClockSnapshot* clocks = packet->set_clock_snapshot();
    // Always reference the boottime timestamps to help trace processor
    // translate the clocks to boottime more efficiently.
    ClockSnapshot::Clock* clock_reference = clocks->add_clocks();
    clock_reference->set_clock_id(ClockSnapshot::Clock::BOOTTIME);
    if (kTraceClockId == ClockSnapshot::Clock::BOOTTIME) {
      clock_reference->set_timestamp(timestamp.since_origin().InNanoseconds());
    } else {
      int64_t current_boot_nanos = TraceBootTicksNow();
      int64_t current_monotonic_nanos =
          TRACE_TIME_TICKS_NOW().since_origin().InNanoseconds();
      int64_t diff = current_boot_nanos - current_monotonic_nanos;
      clock_reference->set_timestamp(timestamp.since_origin().InNanoseconds() +
                                     diff);
    }
    // Absolute clock in micros.
    ClockSnapshot::Clock* clock_absolute = clocks->add_clocks();
    clock_absolute->set_clock_id(kClockIdAbsolute);
    clock_absolute->set_timestamp(timestamp.since_origin().InMicroseconds());
    clock_absolute->set_unit_multiplier_ns(1000u);
    // Delta-encoded incremental clock in micros.
    ClockSnapshot::Clock* clock_incremental = clocks->add_clocks();
    clock_incremental->set_clock_id(kClockIdIncremental);
    clock_incremental->set_timestamp(timestamp.since_origin().InMicroseconds());
    clock_incremental->set_unit_multiplier_ns(1000u);
    clock_incremental->set_is_incremental(true);
  }

  // Emit a new track descriptor in another packet.
  auto packet = trace_writer_->NewTracePacket();
  EmitTrackDescriptor(&packet, trace_event, timestamp);

  reset_incremental_state_ = false;
}

void TrackEventThreadLocalEventSink::SetPacketTimestamp(
    protozero::MessageHandle<TracePacket>* trace_packet,
    base::TimeTicks timestamp,
    bool force_absolute_timestamp) {
  if (force_absolute_timestamp || last_timestamp_ > timestamp) {
    (*trace_packet)->set_timestamp(timestamp.since_origin().InMicroseconds());
    (*trace_packet)->set_timestamp_clock_id(kClockIdAbsolute);
    return;
  }

  // Use default timestamp_clock_id (kClockIdIncremental).
  (*trace_packet)
      ->set_timestamp((timestamp - last_timestamp_).InMicroseconds());
  last_timestamp_ = timestamp;
}

}  // namespace tracing
