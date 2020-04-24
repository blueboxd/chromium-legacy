// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_INSPECTOR_INSPECTOR_MEDIA_CONTEXT_IMPL_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_INSPECTOR_INSPECTOR_MEDIA_CONTEXT_IMPL_H_

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "third_party/blink/public/web/web_media_inspector.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/platform/supplementable.h"

namespace blink {

class Document;
class HTMLMediaElement;

struct MediaPlayer final : public GarbageCollected<MediaPlayer> {
  void Trace(Visitor*) {}

  WebString player_id;
  Vector<InspectorPlayerError> errors;
  Vector<InspectorPlayerEvent> events;
  Vector<InspectorPlayerMessage> messages;
  HashMap<String, InspectorPlayerProperty> properties;
};

class CORE_EXPORT MediaInspectorContextImpl final
    : public GarbageCollected<MediaInspectorContextImpl>,
      public Supplement<LocalFrame>,
      public MediaInspectorContext {
  USING_GARBAGE_COLLECTED_MIXIN(MediaInspectorContextImpl);

 public:
  static const char kSupplementName[];
  static void ProvideToLocalFrame(LocalFrame&);

  // Different ways of getting the singleton instance of
  // MediaInspectorContextImpl depending on what things are easily in scope
  // caller side.
  static MediaInspectorContextImpl* FromLocalFrame(LocalFrame*);
  static MediaInspectorContextImpl* FromDocument(const Document&);
  static MediaInspectorContextImpl* FromHtmlMediaElement(
      const HTMLMediaElement&);

  explicit MediaInspectorContextImpl(LocalFrame&);

  // MediaInspectorContext methods.
  WebString CreatePlayer() override;
  void NotifyPlayerErrors(WebString playerId,
                          const InspectorPlayerErrors&) override;
  void NotifyPlayerEvents(WebString playerId,
                          const InspectorPlayerEvents&) override;
  void NotifyPlayerMessages(WebString playerId,
                            const InspectorPlayerMessages&) override;
  void SetPlayerProperties(WebString playerId,
                           const InspectorPlayerProperties&) override;

  // GarbageCollected methods.
  void Trace(Visitor*) override;

  Vector<WebString> AllPlayerIds();
  const MediaPlayer& MediaPlayerFromId(const WebString&);

 private:
  HeapHashMap<String, Member<MediaPlayer>> players_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_INSPECTOR_INSPECTOR_MEDIA_CONTEXT_IMPL_H_
