// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/gpu/webgpu_image_bitmap_handler.h"

#include "base/memory/scoped_refptr.h"
#include "base/test/null_task_runner.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "components/viz/common/resources/transferable_resource.h"
#include "components/viz/test/test_context_provider.h"
#include "gpu/command_buffer/client/webgpu_interface_stub.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/graphics/accelerated_static_bitmap_image.h"
#include "third_party/blink/renderer/platform/graphics/gpu/drawing_buffer_test_helpers.h"
#include "third_party/blink/renderer/platform/graphics/gpu/shared_gpu_context.h"
#include "third_party/blink/renderer/platform/graphics/static_bitmap_image.h"
#include "third_party/blink/renderer/platform/graphics/test/gpu_test_utils.h"

using testing::_;
using testing::Return;

namespace blink {

static constexpr uint64_t kMaxArrayLength = 40000;

namespace {
gpu::SyncToken GenTestSyncToken(GLbyte id) {
  gpu::SyncToken token;
  // Store id in the first byte
  reinterpret_cast<GLbyte*>(&token)[0] = id;
  return token;
}

scoped_refptr<StaticBitmapImage> CreateBitmap() {
  auto mailbox = gpu::Mailbox::GenerateForSharedImage();
  auto release_callback = viz::SingleReleaseCallback::Create(
      base::BindOnce([](const gpu::SyncToken&, bool) {}));
  return AcceleratedStaticBitmapImage::CreateFromCanvasMailbox(
      mailbox, GenTestSyncToken(100), 0, SkImageInfo::MakeN32Premul(100, 100),
      GL_TEXTURE_2D, true, SharedGpuContext::ContextProviderWrapper(),
      base::PlatformThread::CurrentRef(),
      base::MakeRefCounted<base::NullTaskRunner>(),
      std::move(release_callback));
}

bool GPUUploadingPathSupported() {
// In current state, only passthrough command buffer can work on this path.
// and Windows is the platform that is using passthrough command buffer by
// default.
// TODO(shaobo.yan@intel.com): Enable test on more platforms when they're ready.
#if defined(OS_WIN)
  return true;
#else
  return false;
#endif  // defined(OS_WIN)
}

class MockWebGPUInterface : public gpu::webgpu::WebGPUInterfaceStub {
 public:
  MOCK_METHOD(gpu::webgpu::ReservedTexture,
              ReserveTexture,
              (uint64_t device_client_id));
  MOCK_METHOD(void,
              AssociateMailbox,
              (GLuint64 device_client_id,
               GLuint device_generation,
               GLuint id,
               GLuint generation,
               GLuint usage,
               const GLbyte* mailbox));
  MOCK_METHOD(void,
              DissociateMailbox,
              (GLuint64 device_client_id,
               GLuint texture_id,
               GLuint texture_generation));
};
}  // anonymous namespace

class WebGPUImageBitmapHandlerTest : public testing::Test {
 protected:
  void SetUp() override {}

  void VerifyCopyBytesForWebGPU(uint64_t width,
                                uint64_t height,
                                SkImageInfo info,
                                CanvasColorParams param,
                                IntRect copyRect) {
    const uint64_t content_length = width * height * param.BytesPerPixel();
    std::array<uint8_t, kMaxArrayLength> contents = {0};
    // Initialize contents.
    for (size_t i = 0; i < content_length; ++i) {
      contents[i] = i % std::numeric_limits<uint8_t>::max();
    }

    sk_sp<SkData> image_pixels =
        SkData::MakeWithCopy(contents.data(), content_length);
    scoped_refptr<StaticBitmapImage> image =
        StaticBitmapImage::Create(std::move(image_pixels), info);

    WebGPUImageUploadSizeInfo wgpu_info =
        ComputeImageBitmapWebGPUUploadSizeInfo(copyRect, param);

    const uint64_t result_length = wgpu_info.size_in_bytes;
    std::array<uint8_t, kMaxArrayLength> results = {0};
    bool success = CopyBytesFromImageBitmapForWebGPU(
        image, base::span<uint8_t>(results.data(), result_length), copyRect,
        param);
    ASSERT_EQ(success, true);

    // Compare content and results
    uint32_t row_pitch = wgpu_info.wgpu_row_pitch;
    uint32_t content_row_index =
        (copyRect.Y() * width + copyRect.X()) * param.BytesPerPixel();
    uint32_t result_row_index = 0;
    for (int i = 0; i < copyRect.Height(); ++i) {
      EXPECT_EQ(0,
                memcmp(&contents[content_row_index], &results[result_row_index],
                       copyRect.Width() * param.BytesPerPixel()));
      content_row_index += width * param.BytesPerPixel();
      result_row_index += row_pitch;
    }
  }
};

// Test calculate size
TEST_F(WebGPUImageBitmapHandlerTest, VerifyGetWGPUResourceInfo) {
  uint64_t imageWidth = 63;
  uint64_t imageHeight = 1;
  CanvasColorParams param(CanvasColorSpace::kSRGB, CanvasPixelFormat::kRGBA8,
                          OpacityMode::kNonOpaque);

  // Prebaked expected values.
  uint32_t expected_row_pitch = 256;
  uint64_t expected_size = 256;

  IntRect test_rect(0, 0, imageWidth, imageHeight);
  WebGPUImageUploadSizeInfo info =
      ComputeImageBitmapWebGPUUploadSizeInfo(test_rect, param);
  ASSERT_EQ(expected_size, info.size_in_bytes);
  ASSERT_EQ(expected_row_pitch, info.wgpu_row_pitch);
}

// Copy full image bitmap test
TEST_F(WebGPUImageBitmapHandlerTest, VerifyCopyBytesFromImageBitmapForWebGPU) {
  uint64_t imageWidth = 4;
  uint64_t imageHeight = 2;
  SkImageInfo info = SkImageInfo::Make(
      imageWidth, imageHeight, SkColorType::kRGBA_8888_SkColorType,
      SkAlphaType::kUnpremul_SkAlphaType, SkColorSpace::MakeSRGB());

  IntRect image_data_rect(0, 0, imageWidth, imageHeight);
  CanvasColorParams color_params(CanvasColorSpace::kSRGB,
                                 CanvasPixelFormat::kRGBA8,
                                 OpacityMode::kNonOpaque);
  VerifyCopyBytesForWebGPU(imageWidth, imageHeight, info, color_params,
                           image_data_rect);
}

// Copy sub image bitmap test
TEST_F(WebGPUImageBitmapHandlerTest, VerifyCopyBytesFromSubImageBitmap) {
  uint64_t imageWidth = 63;
  uint64_t imageHeight = 4;
  SkImageInfo info = SkImageInfo::Make(
      imageWidth, imageHeight, SkColorType::kRGBA_8888_SkColorType,
      SkAlphaType::kUnpremul_SkAlphaType, SkColorSpace::MakeSRGB());

  IntRect image_data_rect(2, 2, 60, 2);
  CanvasColorParams color_params(CanvasColorSpace::kSRGB,
                                 CanvasPixelFormat::kRGBA8,
                                 OpacityMode::kNonOpaque);
  VerifyCopyBytesForWebGPU(imageWidth, imageHeight, info, color_params,
                           image_data_rect);
}

class DawnTextureFromImageBitmapTest : public testing::Test {
 protected:
  void SetUp() override {
    auto webgpu = std::make_unique<MockWebGPUInterface>();
    webgpu_ = webgpu.get();
    auto provider = std::make_unique<WebGraphicsContext3DProviderForTests>(
        std::move(webgpu));

    dawn_control_client_ =
        base::MakeRefCounted<DawnControlClientHolder>(std::move(provider));

    dawn_texture_provider_ = base::MakeRefCounted<DawnTextureFromImageBitmap>(
        dawn_control_client_, 1 /* device_client_id */);

    test_context_provider_ = viz::TestContextProvider::Create();
    InitializeSharedGpuContext(test_context_provider_.get());
  }

  void TearDown() override { SharedGpuContext::ResetForTesting(); }
  MockWebGPUInterface* webgpu_;
  scoped_refptr<DawnControlClientHolder> dawn_control_client_;
  scoped_refptr<DawnTextureFromImageBitmap> dawn_texture_provider_;
  scoped_refptr<viz::TestContextProvider> test_context_provider_;
  base::test::TaskEnvironment task_environment_;
};

TEST_F(DawnTextureFromImageBitmapTest, VerifyAccessTexture) {
  if (!GPUUploadingPathSupported()) {
    LOG(ERROR) << "Test skipped because GPU uploading path not supported.";
    return;
  }
  auto bitmap = CreateBitmap();

  viz::TransferableResource resource;
  gpu::webgpu::ReservedTexture reservation = {
      reinterpret_cast<WGPUTexture>(&resource), 1, 1};

  // Test that ProduceDawnTextureFromImageBitmap calls ReserveTexture and
  // AssociateMailbox correctly.
  const GLbyte* mailbox_bytes = nullptr;

  EXPECT_CALL(*webgpu_, ReserveTexture(_)).WillOnce(Return(reservation));
  EXPECT_CALL(*webgpu_, AssociateMailbox(
                            dawn_texture_provider_->GetDeviceClientIdForTest(),
                            _, reservation.id, reservation.generation,
                            WGPUTextureUsage_CopySrc, _))
      .WillOnce(testing::SaveArg<5>(&mailbox_bytes));

  WGPUTexture texture =
      dawn_texture_provider_->ProduceDawnTextureFromImageBitmap(bitmap);

  gpu::Mailbox mailbox = gpu::Mailbox::FromVolatile(
      *reinterpret_cast<const volatile gpu::Mailbox*>(mailbox_bytes));

  EXPECT_TRUE(mailbox == bitmap->GetMailboxHolder().mailbox);
  EXPECT_NE(texture, nullptr);
  EXPECT_EQ(dawn_texture_provider_->GetTextureIdForTest(), 1u);
  EXPECT_EQ(dawn_texture_provider_->GetTextureGenerationForTest(), 1u);

  // Test that FinishDawnTextureFromImageBitmapAccess calls DissociateMailbox
  // correctly.
  EXPECT_CALL(*webgpu_, DissociateMailbox(
                            dawn_texture_provider_->GetDeviceClientIdForTest(),
                            reservation.id, reservation.generation));

  dawn_texture_provider_->FinishDawnTextureFromImageBitmapAccess();

  EXPECT_EQ(dawn_texture_provider_->GetTextureIdForTest(), 0u);
}
}  // namespace blink
