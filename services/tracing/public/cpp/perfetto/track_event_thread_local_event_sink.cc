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
#include "services/tracing/public/cpp/perfetto/traced_value_proto_writer.h"
#include "third_party/perfetto/protos/perfetto/trace/trace_packet.pbzero.h"
#include "third_party/perfetto/protos/perfetto/trace/track_event/debug_annotation.pbzero.h"
#include "third_party/perfetto/protos/perfetto/trace/track_event/log_message.pbzero.h"
#include "third_party/perfetto/protos/perfetto/trace/track_event/process_descriptor.pbzero.h"
#include "third_party/perfetto/protos/perfetto/trace/track_event/source_location.pbzero.h"
#include "third_party/perfetto/protos/perfetto/trace/track_event/task_execution.pbzero.h"

using TraceLog = base::trace_event::TraceLog;
using TrackEvent = perfetto::protos::pbzero::TrackEvent;
using perfetto::protos::pbzero::ThreadDescriptor;

namespace tracing {

namespace {

// Replacement string for names of events with TRACE_EVENT_FLAG_COPY.
const char* const kPrivacyFiltered = "PRIVACY_FILTERED";

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

  std::string json = value->ToString();
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

ThreadDescriptor::ChromeThreadType GetThreadType(
    const char* const thread_name) {
  if (base::MatchPattern(thread_name, "Cr*Main")) {
    return ThreadDescriptor::CHROME_THREAD_MAIN;
  } else if (base::MatchPattern(thread_name, "Chrome*IOThread")) {
    return ThreadDescriptor::CHROME_THREAD_IO;
  } else if (base::MatchPattern(thread_name, "ThreadPoolForegroundWorker*")) {
    return ThreadDescriptor::CHROME_THREAD_POOL_FG_WORKER;
  } else if (base::MatchPattern(thread_name, "ThreadPoolBackgroundWorker*")) {
    return ThreadDescriptor::CHROME_THREAD_POOL_BG_WORKER;
  } else if (base::MatchPattern(thread_name,
                                "ThreadPool*ForegroundBlocking*")) {
    return ThreadDescriptor::CHROME_THREAD_POOL_FB_BLOCKING;
  } else if (base::MatchPattern(thread_name,
                                "ThreadPool*BackgroundBlocking*")) {
    return ThreadDescriptor::CHROME_THREAD_POOL_BG_BLOCKING;
  } else if (base::MatchPattern(thread_name, "ThreadPoolService*")) {
    return ThreadDescriptor::CHROME_THREAD_POOL_SERVICE;
  } else if (base::MatchPattern(thread_name, "CompositorTileWorker*")) {
    return ThreadDescriptor::CHROME_THREAD_COMPOSITOR_WORKER;
  } else if (base::MatchPattern(thread_name, "Compositor")) {
    return ThreadDescriptor::CHROME_THREAD_COMPOSITOR;
  } else if (base::MatchPattern(thread_name, "VizCompositor*")) {
    return ThreadDescriptor::CHROME_THREAD_VIZ_COMPOSITOR;
  } else if (base::MatchPattern(thread_name, "ServiceWorker*")) {
    return ThreadDescriptor::CHROME_THREAD_SERVICE_WORKER;
  } else if (base::MatchPattern(thread_name, "MemoryInfra")) {
    return ThreadDescriptor::CHROME_THREAD_MEMORY_INFRA;
  } else if (base::MatchPattern(thread_name, "StackSamplingProfiler")) {
    return ThreadDescriptor::CHROME_THREAD_SAMPLING_PROFILER;
  }
  return ThreadDescriptor::CHROME_THREAD_UNSPECIFIED;
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
    std::unique_ptr<perfetto::StartupTraceWriter> trace_writer,
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

void TrackEventThreadLocalEventSink::PrepareTrackEvent(
    base::trace_event::TraceEvent* trace_event,
    base::trace_event::TraceEventHandle* handle,
    TrackEvent* track_event) {
  // Each event's updates to InternedData are flushed at the end of
  // AddTraceEvent().
  DCHECK(pending_interning_updates_.empty());

  char phase = trace_event->phase();

  // Split COMPLETE events into BEGIN/END pairs. We write the BEGIN here, and
  // the END in UpdateDuration().
  if (phase == TRACE_EVENT_PHASE_COMPLETE) {
    phase = TRACE_EVENT_PHASE_BEGIN;
  }

  bool is_end_of_a_complete_event = !handle && phase == TRACE_EVENT_PHASE_END;

  uint32_t flags = trace_event->flags();
  bool copy_strings = flags & TRACE_EVENT_FLAG_COPY;
  bool is_java_event = flags & TRACE_EVENT_FLAG_JAVA_STRING_LITERALS;
  bool explicit_timestamp = flags & TRACE_EVENT_FLAG_EXPLICIT_TIMESTAMP;

  const char* category_name =
      TraceLog::GetCategoryGroupName(trace_event->category_group_enabled());
  InterningIndexEntry interned_category{};
  // Don't write the category for END events that were split from a COMPLETE
  // event to save trace size.
  if (!is_end_of_a_complete_event) {
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

  // Don't write the name or arguments for END events that were split from a
  // COMPLETE event to save trace size.
  const char* trace_event_name = trace_event->name();
  if (!is_end_of_a_complete_event) {
    if (copy_strings) {
      if (!is_java_event && privacy_filtering_enabled_) {
        trace_event_name = kPrivacyFiltered;
        interned_name = interned_event_names_.LookupOrAdd(trace_event_name);
      } else {
        interned_name =
            interned_event_names_.LookupOrAdd(std::string(trace_event_name));
        for (size_t i = 0;
             i < trace_event->arg_size() && trace_event->arg_name(i); ++i) {
          interned_annotation_names[i] = interned_annotation_names_.LookupOrAdd(
              std::string(trace_event->arg_name(i)));
        }
      }
    } else {
      interned_name = interned_event_names_.LookupOrAdd(trace_event->name());

      // TODO(eseckler): Remove special handling of typed events here once we
      // support them in TRACE_EVENT macros.

      if (flags & TRACE_EVENT_FLAG_TYPED_PROTO_ARGS) {
        if (trace_event->arg_size() == 2u) {
          DCHECK_EQ(strcmp(category_name, kTaskExecutionEventCategory), 0);
          DCHECK(
              strcmp(trace_event->name(), kTaskExecutionEventNames[0]) == 0 ||
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
  }

  // Events for different processes/threads always use an absolute timestamp.
  bool force_absolute_timestamp =
      ((flags & TRACE_EVENT_FLAG_HAS_PROCESS_ID) &&
       trace_event->process_id() != base::kNullProcessId) ||
      thread_id_ != trace_event->thread_id() || explicit_timestamp;

  if (force_absolute_timestamp || last_timestamp_ > trace_event->timestamp()) {
    track_event->set_timestamp_absolute_us(
        trace_event->timestamp().since_origin().InMicroseconds());
  } else {
    track_event->set_timestamp_delta_us(
        (trace_event->timestamp() - last_timestamp_).InMicroseconds());
    last_timestamp_ = trace_event->timestamp();
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

  // TODO(eseckler): Also convert instant / async / flow events to corresponding
  // native TrackEvent types. Instants & asyncs require using track descriptors
  // rather than instant event scopes / async event IDs.
  TrackEvent::Type track_event_type;
  switch (phase) {
    case TRACE_EVENT_PHASE_BEGIN:
      track_event_type = TrackEvent::TYPE_SLICE_BEGIN;
      break;
    case TRACE_EVENT_PHASE_END:
      track_event_type = TrackEvent::TYPE_SLICE_END;
      break;
    default:
      track_event_type = TrackEvent::TYPE_UNSPECIFIED;
      break;
  }

  if (track_event_type != TrackEvent::TYPE_UNSPECIFIED) {
    track_event->set_type(track_event_type);
  } else {
    legacy_event.GetOrCreate()->set_phase(phase);
  }

  // TODO(eseckler): Convert instant event scopes to tracks.
  if (phase == TRACE_EVENT_PHASE_INSTANT) {
    switch (flags & TRACE_EVENT_FLAG_SCOPE_MASK) {
      case TRACE_EVENT_SCOPE_GLOBAL:
        legacy_event.GetOrCreate()->set_instant_event_scope(
            TrackEvent::LegacyEvent::SCOPE_GLOBAL);
        break;

      case TRACE_EVENT_SCOPE_PROCESS:
        legacy_event.GetOrCreate()->set_instant_event_scope(
            TrackEvent::LegacyEvent::SCOPE_PROCESS);
        break;

      case TRACE_EVENT_SCOPE_THREAD:
        legacy_event.GetOrCreate()->set_instant_event_scope(
            TrackEvent::LegacyEvent::SCOPE_THREAD);
        break;
    }
  }

  uint32_t id_flags =
      flags & (TRACE_EVENT_FLAG_HAS_ID | TRACE_EVENT_FLAG_HAS_LOCAL_ID |
               TRACE_EVENT_FLAG_HAS_GLOBAL_ID);
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

  uint32_t flow_flags =
      flags & (TRACE_EVENT_FLAG_FLOW_OUT | TRACE_EVENT_FLAG_FLOW_IN);
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

  if ((flags & TRACE_EVENT_FLAG_HAS_PROCESS_ID) &&
      trace_event->process_id() != base::kNullProcessId) {
    legacy_event.GetOrCreate()->set_pid_override(trace_event->process_id());
    legacy_event.GetOrCreate()->set_tid_override(-1);
  } else if (thread_id_ != trace_event->thread_id()) {
    legacy_event.GetOrCreate()->set_tid_override(trace_event->thread_id());
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
  EmitThreadDescriptor(&trace_packet, nullptr, true, name);
}

void TrackEventThreadLocalEventSink::EmitThreadDescriptor(
    protozero::MessageHandle<perfetto::protos::pbzero::TracePacket>*
        trace_packet,
    base::trace_event::TraceEvent* trace_event,
    bool explicit_timestamp,
    const char* maybe_new_name) {
  auto* thread_descriptor = (*trace_packet)->set_thread_descriptor();
  thread_descriptor->set_pid(process_id_);
  thread_descriptor->set_tid(thread_id_);

  if (!maybe_new_name) {
    maybe_new_name =
        base::ThreadIdNameManager::GetInstance()->GetNameForCurrentThread();
  }
  if (maybe_new_name && *maybe_new_name &&
      base::StringPiece(thread_name_) != maybe_new_name) {
    thread_name_ = maybe_new_name;
    thread_type_ = GetThreadType(maybe_new_name);
  }
  // TODO(ssid): Adding name and type to thread descriptor adds thread names
  // from killed processes. The catapult trace importer can't handle different
  // processes with same process ids. To workaround this issue, we do not emit
  // name and type when filtering is enabled (when metadata is not emitted).
  // Thread names will be emitted by trace log at metadata generation step when
  // filtering is not enabled. See crbug/978093.
  if (privacy_filtering_enabled_) {
    thread_descriptor->set_chrome_thread_type(thread_type_);
  }

  if (explicit_timestamp || !trace_event) {
    // Don't use a user-provided timestamp as a reference timestamp.
    last_timestamp_ = TRACE_TIME_TICKS_NOW();
  } else {
    last_timestamp_ = trace_event->timestamp();
  }
  if (!trace_event || trace_event->thread_timestamp().is_null()) {
    last_thread_time_ = ThreadNow();
  } else {
    // Thread timestamp is never user-provided.
    DCHECK_LE(trace_event->thread_timestamp(), ThreadNow());
    last_thread_time_ = trace_event->thread_timestamp();
  }
  thread_descriptor->set_reference_timestamp_us(
      last_timestamp_.since_origin().InMicroseconds());
  thread_descriptor->set_reference_thread_time_us(
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
    thread_descriptor->set_reference_thread_instruction_count(
        last_thread_instruction_count_.ToInternalValue());
  }

  // TODO(eseckler): Fill in remaining fields in ThreadDescriptor.
}

void TrackEventThreadLocalEventSink::DoResetIncrementalState(
    base::trace_event::TraceEvent* trace_event,
    bool explicit_timestamp) {
  interned_event_categories_.ResetEmittedState();
  interned_event_names_.ResetEmittedState();
  interned_annotation_names_.ResetEmittedState();
  interned_source_locations_.ResetEmittedState();
  interned_log_message_bodies_.ResetEmittedState();

  // Emit a new thread descriptor in a separate packet, where we also set
  // the |incremental_state_cleared| flag.
  auto trace_packet = trace_writer_->NewTracePacket();
  trace_packet->set_incremental_state_cleared(true);
  EmitThreadDescriptor(&trace_packet, trace_event, explicit_timestamp);
  reset_incremental_state_ = false;
}

}  // namespace tracing
