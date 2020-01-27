// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBCODECS_VIDEO_TRACK_READER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBCODECS_VIDEO_TRACK_READER_H_

#include "third_party/blink/renderer/modules/mediastream/media_stream_track.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"

namespace blink {

class ReadableStream;
class ScriptState;

class MODULES_EXPORT VideoTrackReader final : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static VideoTrackReader* Create(ScriptState* script_state,
                                  MediaStreamTrack* track,
                                  ExceptionState& exception_state);
  explicit VideoTrackReader(ReadableStream* readable);
  ReadableStream* readable() const;

  void Trace(Visitor* visitor) override;

 private:
  VideoTrackReader(const VideoTrackReader&) = delete;
  VideoTrackReader& operator=(const VideoTrackReader&) = delete;

  Member<ReadableStream> readable_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBCODECS_VIDEO_TRACK_READER_H_
