// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBAUDIO_INSPECTOR_WEB_AUDIO_AGENT_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBAUDIO_INSPECTOR_WEB_AUDIO_AGENT_H_

#include <memory>
#include "third_party/blink/renderer/core/inspector/inspector_base_agent.h"
#include "third_party/blink/renderer/core/inspector/protocol/WebAudio.h"
#include "third_party/blink/renderer/modules/modules_export.h"

namespace blink {

class BaseAudioContext;
class Page;

using protocol::WebAudio::ContextRealtimeData;

class MODULES_EXPORT InspectorWebAudioAgent final
    : public InspectorBaseAgent<protocol::WebAudio::Metainfo> {
 public:
  explicit InspectorWebAudioAgent(Page*);
  ~InspectorWebAudioAgent() override;

  // Base Agent methods.
  void Restore() override;

  // Protocol method implementations (agent -> front_end/web_audio)
  protocol::Response enable() override;
  protocol::Response disable() override;
  protocol::Response getRealtimeData(
      const protocol::WebAudio::ContextId&,
      std::unique_ptr<ContextRealtimeData>*) override;

  // API for InspectorInstrumentation (modules/webaudio -> agent)
  void DidCreateBaseAudioContext(BaseAudioContext*);
  void DidDestroyBaseAudioContext(BaseAudioContext*);
  void DidChangeBaseAudioContext(BaseAudioContext*);

  void Trace(blink::Visitor*) override;

 private:
  std::unique_ptr<protocol::WebAudio::BaseAudioContext>
      BuildProtocolContext(BaseAudioContext*);

  Member<Page> page_;
  InspectorAgentState::Boolean enabled_;
  DISALLOW_COPY_AND_ASSIGN(InspectorWebAudioAgent);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBAUDIO_INSPECTOR_WEB_AUDIO_AGENT_H_
