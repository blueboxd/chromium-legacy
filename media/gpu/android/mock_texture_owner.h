// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_ANDROID_MOCK_TEXTURE_OWNER_H_
#define MEDIA_GPU_ANDROID_MOCK_TEXTURE_OWNER_H_

#include <memory>

#include "base/android/scoped_hardware_buffer_fence_sync.h"
#include "media/gpu/android/codec_buffer_wait_coordinator.h"
#include "media/gpu/android/texture_owner.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_surface.h"

using testing::NiceMock;

namespace media {

// This is a mock with a small amount of fake functionality too.
class MockTextureOwner : public TextureOwner {
 public:
  MockTextureOwner(GLuint fake_texture_id,
                   gl::GLContext* fake_context,
                   gl::GLSurface* fake_surface,
                   bool binds_texture_on_update = false);

  MOCK_CONST_METHOD0(GetTextureId, GLuint());
  MOCK_CONST_METHOD0(GetContext, gl::GLContext*());
  MOCK_CONST_METHOD0(GetSurface, gl::GLSurface*());
  MOCK_CONST_METHOD0(CreateJavaSurface, gl::ScopedJavaSurface());
  MOCK_METHOD0(UpdateTexImage, void());
  MOCK_METHOD0(EnsureTexImageBound, void());
  MOCK_METHOD1(GetTransformMatrix, void(float mtx[16]));
  MOCK_METHOD0(ReleaseBackBuffers, void());
  MOCK_METHOD1(OnTextureDestroyed, void(gpu::gles2::AbstractTexture*));
  MOCK_METHOD1(SetFrameAvailableCallback, void(const base::RepeatingClosure&));

  std::unique_ptr<base::android::ScopedHardwareBufferFenceSync>
  GetAHardwareBuffer() override {
    get_a_hardware_buffer_count++;
    return nullptr;
  }

  gl::GLContext* fake_context;
  gl::GLSurface* fake_surface;
  int get_a_hardware_buffer_count = 0;
  bool expect_update_tex_image;

 protected:
  ~MockTextureOwner();
};

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

#endif  // MEDIA_GPU_ANDROID_MOCK_TEXTURE_OWNER_H_
