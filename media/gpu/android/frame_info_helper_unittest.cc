// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/android/frame_info_helper.h"

#include "base/test/bind_test_util.h"
#include "base/test/task_environment.h"
#include "gpu/command_buffer/service/mock_texture_owner.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::Mock;
using testing::SetArgPointee;

namespace media {
namespace {
constexpr gfx::Size kTestVisibleSize(100, 100);
constexpr gfx::Size kTestVisibleSize2(110, 110);
constexpr gfx::Size kTestCodedSize(128, 128);
}  // namespace

class FrameInfoHelperTest : public testing::Test {
 public:
  FrameInfoHelperTest() : helper_(FrameInfoHelper::CreateForTesting()) {}

 protected:
  void GetFrameInfo(
      std::unique_ptr<CodecOutputBufferRenderer> buffer_renderer) {
    const auto* buffer_renderer_raw = buffer_renderer.get();
    bool called = false;
    auto callback = base::BindLambdaForTesting(
        [&](std::unique_ptr<CodecOutputBufferRenderer> buffer_renderer,
            FrameInfoHelper::FrameInfo info, bool success) {
          ASSERT_EQ(buffer_renderer_raw, buffer_renderer.get());
          called = true;
          last_get_frame_info_succeeded_ = success;
          last_frame_info_ = info;
        });
    helper_->GetFrameInfo(std::move(buffer_renderer), callback);
    ASSERT_TRUE(called);
  }

  std::unique_ptr<CodecOutputBufferRenderer> CreateBufferRenderer(
      gfx::Size size,
      scoped_refptr<gpu::TextureOwner> texture_owner) {
    auto codec_buffer_wait_coordinator =
        texture_owner
            ? base::MakeRefCounted<CodecBufferWaitCoordinator>(texture_owner)
            : nullptr;
    auto buffer = CodecOutputBuffer::CreateForTesting(0, size);
    auto buffer_renderer = std::make_unique<CodecOutputBufferRenderer>(
        std::move(buffer), codec_buffer_wait_coordinator);

    // We don't have codec, so releasing test buffer is not possible. Mark it as
    // rendered for test purpose.
    buffer_renderer->set_phase_for_testing(
        CodecOutputBufferRenderer::Phase::kInFrontBuffer);
    return buffer_renderer;
  }

  void FailNextRender(CodecOutputBufferRenderer* buffer_renderer) {
    buffer_renderer->set_phase_for_testing(
        CodecOutputBufferRenderer::Phase::kInvalidated);
  }

  base::test::SingleThreadTaskEnvironment task_environment_;

  std::unique_ptr<FrameInfoHelper> helper_;
  bool last_get_frame_info_succeeded_ = false;
  FrameInfoHelper::FrameInfo last_frame_info_;
};

TEST_F(FrameInfoHelperTest, NoBufferRenderer) {
  // If there is no buffer renderer we shouldn't crash and report that request
  // failed.
  GetFrameInfo(nullptr);
  EXPECT_FALSE(last_get_frame_info_succeeded_);
}

TEST_F(FrameInfoHelperTest, TextureOwner) {
  auto texture_owner = base::MakeRefCounted<NiceMock<gpu::MockTextureOwner>>(
      0, nullptr, nullptr, true);

  // Return CodedSize when GetCodedSizeAndVisibleRect is called.
  ON_CALL(*texture_owner, GetCodedSizeAndVisibleRect(_, _, _))
      .WillByDefault(SetArgPointee<1>(kTestCodedSize));

  // Fail rendering buffer.
  auto buffer1 = CreateBufferRenderer(kTestVisibleSize, texture_owner);
  FailNextRender(buffer1.get());
  // GetFrameInfo should fallback to visible size in this case, but mark request
  // as failed.
  EXPECT_CALL(*texture_owner, GetCodedSizeAndVisibleRect(_, _, _)).Times(0);
  GetFrameInfo(std::move(buffer1));
  EXPECT_FALSE(last_get_frame_info_succeeded_);
  EXPECT_EQ(last_frame_info_.coded_size, kTestVisibleSize);
  Mock::VerifyAndClearExpectations(texture_owner.get());

  // This time rendering should succeed. We expect GetCodedSizeAndVisibleRect to
  // be called and result should be kTestCodedSize instead of kTestVisibleSize.
  EXPECT_CALL(*texture_owner, GetCodedSizeAndVisibleRect(_, _, _)).Times(1);
  GetFrameInfo(CreateBufferRenderer(kTestVisibleSize, texture_owner));
  EXPECT_TRUE(last_get_frame_info_succeeded_);
  EXPECT_EQ(last_frame_info_.coded_size, kTestCodedSize);
  Mock::VerifyAndClearExpectations(texture_owner.get());

  // Verify that we don't render frame on subsequent calls with the same visible
  // size. GetCodedSizeAndVisibleRect should not be called.
  EXPECT_CALL(*texture_owner, GetCodedSizeAndVisibleRect(_, _, _)).Times(0);
  GetFrameInfo(CreateBufferRenderer(kTestVisibleSize, texture_owner));
  EXPECT_TRUE(last_get_frame_info_succeeded_);
  EXPECT_EQ(last_frame_info_.coded_size, kTestCodedSize);
  Mock::VerifyAndClearExpectations(texture_owner.get());

  // Verify that we render if the visible size changed.
  EXPECT_CALL(*texture_owner, GetCodedSizeAndVisibleRect(_, _, _)).Times(1);
  GetFrameInfo(CreateBufferRenderer(kTestVisibleSize2, texture_owner));
  EXPECT_TRUE(last_get_frame_info_succeeded_);
  EXPECT_EQ(last_frame_info_.coded_size, kTestCodedSize);
}

TEST_F(FrameInfoHelperTest, Overlay) {
  // In overlay case we always use visible size.
  GetFrameInfo(CreateBufferRenderer(kTestVisibleSize, nullptr));
  EXPECT_TRUE(last_get_frame_info_succeeded_);
  EXPECT_EQ(last_frame_info_.coded_size, kTestVisibleSize);
}

}  // namespace media
