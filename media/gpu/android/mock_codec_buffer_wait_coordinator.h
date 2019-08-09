// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_ANDROID_MOCK_CODEC_BUFFER_WAIT_COORDINATOR_H_
#define MEDIA_GPU_ANDROID_MOCK_CODEC_BUFFER_WAIT_COORDINATOR_H_

#include "media/gpu/android/codec_buffer_wait_coordinator.h"
#include "media/gpu/android/mock_texture_owner.h"

namespace media {

// Mock class with mostly fake functions.
class MockCodecBufferWaitCoordinator : public CodecBufferWaitCoordinator {
 public:
  MockCodecBufferWaitCoordinator(
      scoped_refptr<NiceMock<MockTextureOwner>> texture_owner);

  MOCK_CONST_METHOD0(texture_owner,
                     scoped_refptr<NiceMock<MockTextureOwner>>());
  MOCK_METHOD0(SetReleaseTimeToNow, void());
  MOCK_METHOD0(IsExpectingFrameAvailable, bool());
  MOCK_METHOD0(WaitForFrameAvailable, void());

  // Fake implementations that the mocks will call by default.
  void FakeSetReleaseTimeToNow() { expecting_frame_available = true; }
  bool FakeIsExpectingFrameAvailable() { return expecting_frame_available; }
  void FakeWaitForFrameAvailable() { expecting_frame_available = false; }

  scoped_refptr<NiceMock<MockTextureOwner>> mock_texture_owner;
  bool expecting_frame_available;

 protected:
  ~MockCodecBufferWaitCoordinator();
};

}  // namespace media

#endif  // MEDIA_GPU_ANDROID_MOCK_CODEC_BUFFER_WAIT_COORDINATOR_H_
