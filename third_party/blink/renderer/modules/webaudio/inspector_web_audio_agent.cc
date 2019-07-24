// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webaudio/inspector_web_audio_agent.h"

#include <memory>
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/modules/webaudio/base_audio_context.h"
#include "third_party/blink/renderer/modules/webaudio/audio_context.h"
#include "third_party/blink/renderer/modules/webaudio/audio_graph_tracer.h"

namespace blink {

namespace {

String GetContextTypeEnum(BaseAudioContext* context) {
  return context->HasRealtimeConstraint()
      ? protocol::WebAudio::ContextTypeEnum::Realtime
      : protocol::WebAudio::ContextTypeEnum::Offline;
}

String GetContextStateEnum(BaseAudioContext* context) {
  switch (context->ContextState()) {
    case BaseAudioContext::AudioContextState::kSuspended:
      return protocol::WebAudio::ContextStateEnum::Suspended;
    case BaseAudioContext::AudioContextState::kRunning:
      return protocol::WebAudio::ContextStateEnum::Running;
    case BaseAudioContext::AudioContextState::kClosed:
      return protocol::WebAudio::ContextStateEnum::Closed;
    default:
      NOTREACHED();
      break;
  }
}

}  // namespace

using protocol::Response;

InspectorWebAudioAgent::InspectorWebAudioAgent(Page* page)
    : page_(page),
      enabled_(&agent_state_, /*default_value=*/false) {
}

InspectorWebAudioAgent::~InspectorWebAudioAgent() = default;

void InspectorWebAudioAgent::Restore() {
  if (!enabled_.Get())
    return;

  AudioGraphTracer* graph_tracer = AudioGraphTracer::FromPage(page_);
  graph_tracer->SetInspectorAgent(this);
}

Response InspectorWebAudioAgent::enable() {
  if (enabled_.Get())
    return Response::OK();
  enabled_.Set(true);
  AudioGraphTracer* graph_tracer = AudioGraphTracer::FromPage(page_);
  graph_tracer->SetInspectorAgent(this);
  return Response::OK();
}

Response InspectorWebAudioAgent::disable() {
  if (!enabled_.Get())
    return Response::OK();
  enabled_.Clear();
  AudioGraphTracer* graph_tracer = AudioGraphTracer::FromPage(page_);
  graph_tracer->SetInspectorAgent(nullptr);
  return Response::OK();
}

Response InspectorWebAudioAgent::getRealtimeData(
    const protocol::WebAudio::ContextId& contextId,
    std::unique_ptr<ContextRealtimeData>* out_data) {
  auto* const graph_tracer = AudioGraphTracer::FromPage(page_);
  if (!enabled_.Get())
    return Response::Error("Enable agent first.");

  BaseAudioContext* context = graph_tracer->GetContextById(contextId);
  if (!context)
    return Response::Error("Cannot find BaseAudioContext with such id.");

  if (!context->HasRealtimeConstraint()) {
    return Response::Error(
        "ContextRealtimeData is only avaliable for an AudioContext.");
  }

  // The realtime metric collection is only for AudioContext.
  AudioCallbackMetric metric =
      static_cast<AudioContext*>(context)->GetCallbackMetric();
  *out_data = ContextRealtimeData::create()
          .setCurrentTime(context->currentTime())
          .setRenderCapacity(metric.render_capacity)
          .setCallbackIntervalMean(metric.mean_callback_interval)
          .setCallbackIntervalVariance(metric.variance_callback_interval)
          .build();
  return Response::OK();
}

void InspectorWebAudioAgent::DidCreateBaseAudioContext(
    BaseAudioContext* context) {
  GetFrontend()->contextCreated(BuildProtocolContext(context));
}

void InspectorWebAudioAgent::DidDestroyBaseAudioContext(
    BaseAudioContext* context) {
  GetFrontend()->contextDestroyed(context->Uuid());
}

void InspectorWebAudioAgent::DidChangeBaseAudioContext(
    BaseAudioContext* context) {
  GetFrontend()->contextChanged(BuildProtocolContext(context));
}

std::unique_ptr<protocol::WebAudio::BaseAudioContext>
InspectorWebAudioAgent::BuildProtocolContext(BaseAudioContext* context) {
  return protocol::WebAudio::BaseAudioContext::create()
      .setContextId(context->Uuid())
      .setContextType(GetContextTypeEnum(context))
      .setContextState(GetContextStateEnum(context))
      .setCallbackBufferSize(context->CallbackBufferSize())
      .setMaxOutputChannelCount(context->MaxChannelCount())
      .setSampleRate(context->sampleRate())
      .build();
}

void InspectorWebAudioAgent::Trace(blink::Visitor* visitor) {
  visitor->Trace(page_);
  InspectorBaseAgent::Trace(visitor);
}

}  // namespace blink
