// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/android/video_frame_factory_impl.h"

#include "base/bind.h"
#include "base/single_thread_task_runner.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_task_environment.h"
#include "base/threading/thread_task_runner_handle.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "gpu/config/gpu_preferences.h"
#include "media/base/limits.h"
#include "media/gpu/android/maybe_render_early_manager.h"
#include "media/gpu/android/mock_codec_image.h"
#include "media/gpu/android/shared_image_video_provider.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::test::RunOnceCallback;
using testing::_;
using testing::SaveArg;

namespace gpu {
class CommandBufferStub;
}  // namespace gpu

namespace media {

class MockMaybeRenderEarlyManager : public MaybeRenderEarlyManager {
 public:
  MOCK_METHOD1(SetSurfaceBundle, void(scoped_refptr<CodecSurfaceBundle>));
  MOCK_METHOD1(AddCodecImage, void(scoped_refptr<CodecImageHolder>));
  MOCK_METHOD0(MaybeRenderEarly, void());
};

class MockSharedImageVideoProvider : public SharedImageVideoProvider {
 public:
  MockSharedImageVideoProvider() : spec_(gfx::Size(0, 0)) {}

  void Initialize(GpuInitCB gpu_init_cb) { Initialize_(gpu_init_cb); }

  MOCK_METHOD1(Initialize_, void(GpuInitCB& gpu_init_cb));

  void RequestImage(ImageReadyCB cb,
                    const ImageSpec& spec,
                    scoped_refptr<TextureOwner> texture_owner) override {
    cb_ = std::move(cb);
    spec_ = spec;
    texture_owner_ = std::move(texture_owner);

    MockRequestImage();
  }

  MOCK_METHOD0(MockRequestImage, void());

  // Most recent arguments to RequestImage.
  ImageReadyCB cb_;
  ImageSpec spec_;
  scoped_refptr<TextureOwner> texture_owner_;
};

class VideoFrameFactoryImplTest : public testing::Test {
 public:
  VideoFrameFactoryImplTest()
      : task_runner_(base::ThreadTaskRunnerHandle::Get()) {
    auto get_stub_cb = base::BindRepeating(
        []() -> gpu::CommandBufferStub* { return nullptr; });

    auto image_provider = std::make_unique<MockSharedImageVideoProvider>();
    image_provider_raw_ = image_provider.get();

    auto mre_manager = std::make_unique<MockMaybeRenderEarlyManager>();
    mre_manager_raw_ = mre_manager.get();

    impl_ = std::make_unique<VideoFrameFactoryImpl>(
        task_runner_, gpu_preferences_, std::move(image_provider),
        std::move(mre_manager));
  }
  ~VideoFrameFactoryImplTest() override = default;

  void RequestVideoFrame() {
    gfx::Size coded_size(100, 100);
    gfx::Rect visible_rect(coded_size);
    gfx::Size natural_size(coded_size);
    auto output_buffer = CodecOutputBuffer::CreateForTesting(0, coded_size);
    ASSERT_TRUE(
        VideoFrame::IsValidConfig(PIXEL_FORMAT_ARGB, VideoFrame::STORAGE_OPAQUE,
                                  coded_size, visible_rect, natural_size));

    // We should get a call to the output callback, but no calls to the
    // provider.
    // TODO(liberato): Verify that it's sending the proper TextureOwner.
    // However, we haven't actually given it a TextureOwner yet.
    output_buffer_raw_ = output_buffer.get();
    EXPECT_CALL(*image_provider_raw_, MockRequestImage());

    impl_->CreateVideoFrame(
        std::move(output_buffer), base::TimeDelta(), natural_size,
        PromotionHintAggregator::NotifyPromotionHintCB(), output_cb_.Get());
    base::RunLoop().RunUntilIdle();
  }

  base::test::ScopedTaskEnvironment scoped_task_environment_;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  std::unique_ptr<VideoFrameFactoryImpl> impl_;

  MockMaybeRenderEarlyManager* mre_manager_raw_ = nullptr;
  MockSharedImageVideoProvider* image_provider_raw_ = nullptr;

  // Most recently created CodecOutputBuffer.
  CodecOutputBuffer* output_buffer_raw_ = nullptr;

  // Sent to |impl_| by RequestVideoFrame..
  base::MockCallback<VideoFrameFactory::OnceOutputCb> output_cb_;

  gpu::GpuPreferences gpu_preferences_;
};

TEST_F(VideoFrameFactoryImplTest, ImageProviderInitFailure) {
  // If the image provider fails to init, then our init cb should be called with
  // no TextureOwner.
  EXPECT_CALL(*image_provider_raw_, Initialize_(_))
      .Times(1)
      .WillOnce(RunOnceCallback<0>(nullptr));
  base::MockCallback<VideoFrameFactory::InitCb> init_cb;
  EXPECT_CALL(init_cb, Run(scoped_refptr<TextureOwner>(nullptr)));
  impl_->Initialize(VideoFrameFactory::OverlayMode::kDontRequestPromotionHints,
                    init_cb.Get());
  base::RunLoop().RunUntilIdle();

  // TODO(liberato): for testing, we could just skip calling the gpu init cb,
  // since |impl_| doesn't know or care if it's called.  that way, we don't need
  // to mock out making the callback work.  would be nice, though.
}

TEST_F(VideoFrameFactoryImplTest,
       SetSurfaceBundleForwardsToMaybeRenderEarlyManager) {
  // Sending a non-null CodecSurfaceBundle should forward it to |mre_manager|.
  scoped_refptr<CodecSurfaceBundle> surface_bundle =
      base::MakeRefCounted<CodecSurfaceBundle>();
  EXPECT_CALL(*mre_manager_raw_, SetSurfaceBundle(surface_bundle));
  impl_->SetSurfaceBundle(surface_bundle);
  base::RunLoop().RunUntilIdle();
}

TEST_F(VideoFrameFactoryImplTest, CreateVideoFrameFailsIfUnsupportedFormat) {
  // Sending an unsupported format should cause an early failure, without a
  // thread hop.
  gfx::Size coded_size(limits::kMaxDimension + 1, limits::kMaxDimension + 1);
  gfx::Rect visible_rect(coded_size);
  gfx::Size natural_size(0, 0);
  auto output_buffer = CodecOutputBuffer::CreateForTesting(0, coded_size);
  ASSERT_FALSE(VideoFrame::IsValidConfig(PIXEL_FORMAT_ARGB,
                                         VideoFrame::STORAGE_OPAQUE, coded_size,
                                         visible_rect, natural_size));

  // We should get a call to the output callback, but no calls to the provider.
  base::MockCallback<VideoFrameFactory::OnceOutputCb> output_cb;
  EXPECT_CALL(output_cb, Run(scoped_refptr<VideoFrame>(nullptr)));
  EXPECT_CALL(*image_provider_raw_, MockRequestImage()).Times(0);

  impl_->CreateVideoFrame(
      std::move(output_buffer), base::TimeDelta(), natural_size,
      PromotionHintAggregator::NotifyPromotionHintCB(), output_cb.Get());
  base::RunLoop().RunUntilIdle();
}

TEST_F(VideoFrameFactoryImplTest, CreateVideoFrameSucceeds) {
  // Creating a video frame calls through to the image provider, and forwards a
  // VideoFrame to the output cb.
  //
  // TODO(liberato): Consider testing the metadata values.
  RequestVideoFrame();

  // Call the ImageReadyCB.
  scoped_refptr<VideoFrame> frame;
  EXPECT_CALL(output_cb_, Run(_)).WillOnce(SaveArg<0>(&frame));
  SharedImageVideoProvider::ImageRecord record;
  record.mailbox = gpu::Mailbox::Generate();
  bool release_cb_called_flag = false;
  record.release_cb =
      base::BindOnce([](bool* flag, const gpu::SyncToken&) { *flag = true; },
                     base::Unretained(&release_cb_called_flag));
  auto codec_image = base::MakeRefCounted<MockCodecImage>();
  record.codec_image_holder =
      base::MakeRefCounted<CodecImageHolder>(task_runner_, codec_image);
  std::move(image_provider_raw_->cb_).Run(std::move(record));
  base::RunLoop().RunUntilIdle();
  EXPECT_NE(frame, nullptr);

  // Make sure that it set the output buffer properly.
  EXPECT_EQ(codec_image->get_codec_output_buffer_for_testing(),
            output_buffer_raw_);

  // Destroy the VideoFrame, and verify that our release cb is called.
  EXPECT_FALSE(release_cb_called_flag);
  frame = nullptr;
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(release_cb_called_flag);
}

TEST_F(VideoFrameFactoryImplTest,
       DestroyingFactoryDuringVideoFrameCreationDoesntCrash) {
  // We should be able to destroy |impl_| while a VideoFrame is pending, and
  // nothing bad should happen.
  RequestVideoFrame();
  impl_ = nullptr;
  base::RunLoop().RunUntilIdle();
}

}  // namespace media
