// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webcodecs/video_track_reader.h"

#include "media/base/video_frame.h"
#include "third_party/blink/public/web/modules/mediastream/media_stream_video_sink.h"
#include "third_party/blink/public/web/modules/mediastream/media_stream_video_track.h"
#include "third_party/blink/renderer/core/streams/readable_stream.h"
#include "third_party/blink/renderer/core/streams/readable_stream_default_controller_with_script_scope.h"
#include "third_party/blink/renderer/core/streams/underlying_source_base.h"
#include "third_party/blink/renderer/modules/webcodecs/video_frame.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"

namespace blink {

class VideoTrackReadableStreamSource final : public UnderlyingSourceBase,
                                             public MediaStreamVideoSink {
 public:
  VideoTrackReadableStreamSource(ScriptState* script_state,
                                 MediaStreamTrack* track)
      : UnderlyingSourceBase(script_state),
        task_runner_(ExecutionContext::From(script_state)
                         ->GetTaskRunner(TaskType::kInternalMedia)) {
    ConnectToTrack(track->Component(),
                   ConvertToBaseRepeatingCallback(CrossThreadBindRepeating(
                       &VideoTrackReadableStreamSource::OnFrameFromVideoTrack,
                       WrapCrossThreadPersistent(this))),
                   false /* is_sink_secure */);
  }

  // Callback of MediaStreamVideoSink::ConnectToTrack
  void OnFrameFromVideoTrack(scoped_refptr<media::VideoFrame> media_frame,
                             base::TimeTicks estimated_capture_time) {
    // The value of estimated_capture_time here seems to almost always be the
    // system clock and most implementations of this callback ignore it.
    // So, we will also ignore it.
    DCHECK(media_frame);
    PostCrossThreadTask(
        *task_runner_.get(), FROM_HERE,
        CrossThreadBindOnce(
            &VideoTrackReadableStreamSource::OnFrameFromVideoTrackOnTaskRunner,
            WrapCrossThreadPersistent(this), std::move(media_frame)));
  }

  void OnFrameFromVideoTrackOnTaskRunner(
      scoped_refptr<media::VideoFrame> media_frame) {
    Controller()->Enqueue(
        MakeGarbageCollected<VideoFrame>(std::move(media_frame)));
  }

  // MediaStreamVideoSink override
  void OnReadyStateChanged(WebMediaStreamSource::ReadyState state) override {
    if (state == WebMediaStreamSource::kReadyStateEnded)
      Close();
  }

  // UnderlyingSourceBase overrides.
  ScriptPromise pull(ScriptState* script_state) override {
    // Since video tracks are all push-based with no back pressure, there's
    // nothing to do here.
    return ScriptPromise::CastUndefined(script_state);
  }

  ScriptPromise Cancel(ScriptState* script_state, ScriptValue reason) override {
    Close();
    return ScriptPromise::CastUndefined(script_state);
  }

  void ContextDestroyed() override { DisconnectFromTrack(); }

  void Close() {
    if (Controller())
      Controller()->Close();
    DisconnectFromTrack();
  }

  // VideoFrames will be queue on this task runner.
  const scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
};

VideoTrackReader* VideoTrackReader::Create(ScriptState* script_state,
                                           MediaStreamTrack* track,
                                           ExceptionState& exception_state) {
  if (track->kind() != "video") {
    exception_state.ThrowTypeError(
        "Can only read video frames from video tracks.");
    return nullptr;
  }

  if (!script_state->ContextIsValid()) {  // when the context is detached
    exception_state.ThrowDOMException(DOMExceptionCode::kNotSupportedError,
                                      "The context has been destroyed");

    return nullptr;
  }

  auto* source =
      MakeGarbageCollected<VideoTrackReadableStreamSource>(script_state, track);
  auto* readable =
      ReadableStream::CreateWithCountQueueingStrategy(script_state, source, 0);
  return MakeGarbageCollected<VideoTrackReader>(readable);
}

VideoTrackReader::VideoTrackReader(ReadableStream* readable)
    : readable_(readable) {}

ReadableStream* VideoTrackReader::readable() const {
  return readable_;
}

void VideoTrackReader::Trace(Visitor* visitor) {
  visitor->Trace(readable_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
