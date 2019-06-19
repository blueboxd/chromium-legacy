// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/timing/profiler_group.h"

#include "base/time/time.h"
#include "build/build_config.h"
#include "third_party/blink/renderer/bindings/core/v8/profiler_trace_builder.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/timing/profiler.h"
#include "third_party/blink/renderer/core/timing/profiler_init_options.h"
#include "third_party/blink/renderer/core/timing/profiler_trace.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"
#include "v8/include/v8-profiler.h"
#include "v8/include/v8.h"

namespace blink {

namespace {

#if defined(OS_WIN)
// On Windows, assume we have the coarsest possible timer.
static constexpr int kBaseSampleIntervalMs =
    base::Time::kMinLowResolutionThresholdMs;
#else
// Default to a 10ms base sampling interval on other platforms.
// TODO(acomminos): Reevaluate based on empirical overhead.
static constexpr int kBaseSampleIntervalMs = 10;
#endif  // defined(OS_WIN)

}  // namespace

ProfilerGroup* ProfilerGroup::From(v8::Isolate* isolate) {
  auto* isolate_data = V8PerIsolateData::From(isolate);
  auto* profiler_group =
      reinterpret_cast<ProfilerGroup*>(isolate_data->ProfilerGroup());
  if (!profiler_group) {
    profiler_group = MakeGarbageCollected<ProfilerGroup>(isolate);
    isolate_data->SetProfilerGroup(profiler_group);
  }
  return profiler_group;
}

ProfilerGroup::ProfilerGroup(v8::Isolate* isolate)
    : isolate_(isolate),
      cpu_profiler_(v8::CpuProfiler::New(isolate, v8::kStandardNaming)),
      next_profiler_id_(0) {
  DCHECK(RuntimeEnabledFeatures::ExperimentalJSProfilerEnabled());
#if defined(OS_WIN)
  // Avoid busy-waiting on Windows, clamping us to the system clock interrupt
  // interval in the worst case.
  cpu_profiler_->SetUsePreciseSampling(false);
#endif  // defined(OS_WIN)
  cpu_profiler_->SetSamplingInterval(kBaseSampleIntervalMs *
                                     base::Time::kMicrosecondsPerMillisecond);
}

Profiler* ProfilerGroup::CreateProfiler(ScriptState* script_state,
                                        const ProfilerInitOptions& init_options,
                                        base::TimeTicks time_origin,
                                        ExceptionState& exception_state) {
  DCHECK_EQ(script_state->GetIsolate(), isolate_);
  DCHECK(cpu_profiler_);
  DCHECK(init_options.hasSampleInterval());

  if (init_options.sampleInterval() < 0) {
    exception_state.ThrowRangeError("Expected non-negative sample interval");
    return nullptr;
  }

  // TODO(acomminos): Requires support in V8 for subsampling intervals
  int sample_interval_ms = kBaseSampleIntervalMs;

  String profiler_id = NextProfilerId();
  v8::CpuProfilingOptions options(
      v8::kLeafNodeLineNumbers, init_options.hasMaxBufferSize()
                                    ? init_options.maxBufferSize()
                                    : v8::CpuProfilingOptions::kNoSampleLimit);

  cpu_profiler_->StartProfiling(V8String(isolate_, profiler_id), options);

  // Limit non-crossorigin script frames to the origin that started the
  // profiler.
  auto* execution_context = ExecutionContext::From(script_state);
  scoped_refptr<const SecurityOrigin> source_origin(
      execution_context->GetSecurityOrigin());

  auto* profiler = MakeGarbageCollected<Profiler>(
      this, profiler_id, sample_interval_ms, source_origin, time_origin);
  profilers_.insert(profiler);
  return profiler;
}

ProfilerGroup::~ProfilerGroup() {
  // v8::CpuProfiler should have been torn down by WillBeDestroyed.
  DCHECK(!cpu_profiler_);
}

void ProfilerGroup::WillBeDestroyed() {
  for (auto& profiler : profilers_) {
    DCHECK(profiler);
    profiler->Dispose();
    DCHECK(profiler->stopped());
  }

  cpu_profiler_->Dispose();
  cpu_profiler_ = nullptr;
}

void ProfilerGroup::Trace(blink::Visitor* visitor) {
  visitor->Trace(profilers_);
  V8PerIsolateData::GarbageCollectedData::Trace(visitor);
}

void ProfilerGroup::StopProfiler(ScriptState* script_state,
                                 Profiler* profiler,
                                 ScriptPromiseResolver* resolver) {
  DCHECK(cpu_profiler_);
  DCHECK(!profiler->stopped());

  v8::Local<v8::String> profiler_id =
      V8String(isolate_, profiler->ProfilerId());
  auto* profile = cpu_profiler_->StopProfiling(profiler_id);
  auto* trace = ProfilerTraceBuilder::FromProfile(
      script_state, profile, profiler->SourceOrigin(), profiler->TimeOrigin());
  resolver->Resolve(trace);

  profile->Delete();
}

void ProfilerGroup::CancelProfiler(Profiler* profiler) {
  DCHECK(cpu_profiler_);
  DCHECK(!profiler->stopped());

  v8::HandleScope scope(isolate_);
  v8::Local<v8::String> profiler_id =
      V8String(isolate_, profiler->ProfilerId());
  auto* profile = cpu_profiler_->StopProfiling(profiler_id);

  profile->Delete();
}

String ProfilerGroup::NextProfilerId() {
  auto id = String::Format("blink::Profiler[%d]", next_profiler_id_);
  ++next_profiler_id_;
  return id;
}

}  // namespace blink
