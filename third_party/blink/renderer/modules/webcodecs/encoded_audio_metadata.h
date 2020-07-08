// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBCODECS_ENCODED_AUDIO_METADATA_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBCODECS_ENCODED_AUDIO_METADATA_H_

#include "base/time/time.h"

namespace blink {

struct EncodedAudioMetadata {
  bool key_frame = false;
  base::TimeDelta timestamp;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBCODECS_ENCODED_AUDIO_METADATA_H_
