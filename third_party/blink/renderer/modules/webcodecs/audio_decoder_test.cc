// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_audio_decoder_init.h"
#include "third_party/blink/renderer/platform/heap/heap.h"

#include "third_party/blink/renderer/modules/webcodecs/audio_decoder.h"

namespace blink {

TEST(AudioDecoderTest, Construction) {
  V8TestingScope v8_scope;

  auto* init = MakeGarbageCollected<AudioDecoderInit>();

  auto* decoder = MakeGarbageCollected<AudioDecoder>(
      v8_scope.GetScriptState(), init, v8_scope.GetExceptionState());

  EXPECT_EQ(decoder->decodeQueueSize(), 0);
}

}  // namespace blink
