// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_SPEECH_RECOGNITION_CLIENT_H_
#define MEDIA_BASE_SPEECH_RECOGNITION_CLIENT_H_

#include <memory>

#include "media/base/audio_buffer.h"
#include "media/base/media_export.h"

namespace media {

// The interface for the speech recognition client used to transcribe audio into
// captions.
class MEDIA_EXPORT SpeechRecognitionClient {
 public:
  virtual ~SpeechRecognitionClient() = default;

  virtual void AddAudio(scoped_refptr<AudioBuffer> buffer) = 0;

  virtual bool IsSpeechRecognitionAvailable() = 0;
};

}  // namespace media

#endif  // MEDIA_BASE_SPEECH_RECOGNITION_CLIENT_H_
