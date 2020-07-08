// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webcodecs/audio_frame.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_audio_frame_init.h"

namespace blink {

// static
AudioFrame* AudioFrame::Create(AudioFrameInit* init,
                               ExceptionState& exception_state) {
  // FIXME : throw exception if no audio buffer.
  return MakeGarbageCollected<AudioFrame>(init);
}

AudioFrame::AudioFrame(AudioFrameInit* init)
    : timestamp_(init->timestamp()), buffer_(init->buffer()) {}

AudioFrame::AudioFrame(scoped_refptr<media::AudioBuffer> buffer)
    : timestamp_(buffer->timestamp().InMicroseconds()) {
  // FIXME - use the |buffer| to initialize AudioBuffer.
}

void AudioFrame::close() {
  buffer_.Clear();
}

uint64_t AudioFrame::timestamp() const {
  return timestamp_;
}
AudioBuffer* AudioFrame::buffer() const {
  return buffer_;
}

void AudioFrame::Trace(Visitor* visitor) const {
  visitor->Trace(buffer_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
