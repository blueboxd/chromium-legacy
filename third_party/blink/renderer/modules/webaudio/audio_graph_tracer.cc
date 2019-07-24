// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webaudio/audio_graph_tracer.h"

#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/modules/webaudio/base_audio_context.h"
#include "third_party/blink/renderer/modules/webaudio/inspector_web_audio_agent.h"

namespace blink {

const char AudioGraphTracer::kSupplementName[] = "AudioGraphTracer";

void AudioGraphTracer::ProvideAudioGraphTracerTo(Page& page) {
  page.ProvideSupplement(MakeGarbageCollected<AudioGraphTracer>());
}

AudioGraphTracer::AudioGraphTracer()
    : inspector_agent_(nullptr) {}

void AudioGraphTracer::Trace(blink::Visitor* visitor) {
  visitor->Trace(inspector_agent_);
  visitor->Trace(contexts_);
  Supplement<Page>::Trace(visitor);
}

void AudioGraphTracer::SetInspectorAgent(InspectorWebAudioAgent* agent) {
  inspector_agent_ = agent;
  if (!inspector_agent_)
    return;
  for (const auto& context : contexts_)
    inspector_agent_->DidCreateBaseAudioContext(context);
}

void AudioGraphTracer::DidCreateBaseAudioContext(
    BaseAudioContext* context) {
  DCHECK(!contexts_.Contains(context));

  contexts_.insert(context);
  if (inspector_agent_)
    inspector_agent_->DidCreateBaseAudioContext(context);
}

void AudioGraphTracer::DidDestroyBaseAudioContext(
    BaseAudioContext* context) {
  DCHECK(contexts_.Contains(context));

  contexts_.erase(context);
  if (inspector_agent_)
    inspector_agent_->DidDestroyBaseAudioContext(context);
}

void AudioGraphTracer::DidChangeBaseAudioContext(
    BaseAudioContext* context) {
  DCHECK(contexts_.Contains(context));

  if (inspector_agent_)
    inspector_agent_->DidChangeBaseAudioContext(context);
}

BaseAudioContext* AudioGraphTracer::GetContextById(String contextId) {
  for (const auto& context : contexts_) {
    if (context->Uuid() == contextId)
      return context;
  }

  return nullptr;
}

AudioGraphTracer* AudioGraphTracer::FromPage(Page* page) {
  return Supplement<Page>::From<AudioGraphTracer>(page);
}

AudioGraphTracer* AudioGraphTracer::FromDocument(
    const Document& document) {
  return AudioGraphTracer::FromPage(document.GetPage());
}

}  // namespace blink
