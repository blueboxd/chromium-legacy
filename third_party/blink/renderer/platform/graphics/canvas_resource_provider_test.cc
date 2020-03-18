// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/canvas_resource_provider.h"

#include "components/viz/common/resources/single_release_callback.h"
#include "components/viz/test/test_context_provider.h"
#include "components/viz/test/test_gles2_interface.h"
#include "components/viz/test/test_gpu_memory_buffer_manager.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/graphics/canvas_color_params.h"
#include "third_party/blink/renderer/platform/graphics/canvas_resource_dispatcher.h"
#include "third_party/blink/renderer/platform/graphics/gpu/shared_gpu_context.h"
#include "third_party/blink/renderer/platform/graphics/static_bitmap_image.h"
#include "third_party/blink/renderer/platform/graphics/test/fake_gles2_interface.h"
#include "third_party/blink/renderer/platform/graphics/test/fake_web_graphics_context_3d_provider.h"
#include "third_party/blink/renderer/platform/graphics/test/gpu_memory_buffer_test_platform.h"
#include "third_party/blink/renderer/platform/graphics/test/gpu_test_utils.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/skia/include/core/SkFilterQuality.h"
#include "ui/gfx/buffer_types.h"

using testing::_;
using testing::InSequence;
using testing::Return;
using testing::Test;

namespace blink {

namespace {

constexpr int kMaxTextureSize = 1024;

class MockCanvasResourceDispatcherClient
    : public CanvasResourceDispatcherClient {
 public:
  MockCanvasResourceDispatcherClient() = default;

  MOCK_METHOD0(BeginFrame, bool());
  MOCK_METHOD1(SetFilterQualityInResource, void(SkFilterQuality));
};

}  // anonymous namespace

class CanvasResourceProviderTest : public Test {
 public:
  void SetUp() override {
    test_context_provider_ = viz::TestContextProvider::Create();
    auto* test_gl = test_context_provider_->UnboundTestContextGL();
    test_gl->set_max_texture_size(kMaxTextureSize);
    test_gl->set_support_texture_storage_image(true);
    test_gl->set_supports_shared_image_swap_chain(true);
    test_gl->set_supports_gpu_memory_buffer_format(gfx::BufferFormat::RGBA_8888,
                                                   true);
    test_gl->set_supports_gpu_memory_buffer_format(gfx::BufferFormat::BGRA_8888,
                                                   true);
    test_gl->set_supports_gpu_memory_buffer_format(gfx::BufferFormat::RGBA_F16,
                                                   true);
    InitializeSharedGpuContext(test_context_provider_.get(),
                               &image_decode_cache_);
    context_provider_wrapper_ = SharedGpuContext::ContextProviderWrapper();
  }

  void TearDown() override { SharedGpuContext::ResetForTesting(); }

 protected:
  cc::StubDecodeCache image_decode_cache_;
  scoped_refptr<viz::TestContextProvider> test_context_provider_;
  base::WeakPtr<WebGraphicsContext3DProviderWrapper> context_provider_wrapper_;
  ScopedTestingPlatformSupport<GpuMemoryBufferTestPlatform> platform_;
};

TEST_F(CanvasResourceProviderTest, CanvasResourceProviderAcceleratedOverlay) {
  const IntSize kSize(10, 10);
  const CanvasColorParams kColorParams(
      CanvasColorSpace::kSRGB, CanvasColorParams::GetNativeCanvasPixelFormat(),
      kNonOpaque);

  auto provider = CanvasResourceProvider::Create(
      kSize,
      CanvasResourceProvider::ResourceUsage::kAcceleratedDirect2DResourceUsage,
      context_provider_wrapper_, 0 /* msaa_sample_count */,
      kMedium_SkFilterQuality, kColorParams,
      CanvasResourceProvider::kAllowImageChromiumPresentationMode,
      nullptr /* resource_dispatcher */, true /* is_origin_top_left */);

  EXPECT_EQ(provider->Size(), kSize);
  EXPECT_TRUE(provider->IsValid());
  EXPECT_TRUE(provider->IsAccelerated());
  EXPECT_TRUE(provider->SupportsDirectCompositing());
  EXPECT_TRUE(provider->SupportsSingleBuffering());
  EXPECT_EQ(provider->ColorParams().ColorSpace(), kColorParams.ColorSpace());
  // As it is an CanvasResourceProviderSharedImage and an accelerated canvas, it
  // will internally force it to kRGBA8
  EXPECT_EQ(provider->ColorParams().PixelFormat(), CanvasPixelFormat::kRGBA8);
  EXPECT_EQ(provider->ColorParams().GetOpacityMode(),
            kColorParams.GetOpacityMode());

  EXPECT_FALSE(provider->IsSingleBuffered());
  provider->TryEnableSingleBuffering();
  EXPECT_TRUE(provider->IsSingleBuffered());
}

TEST_F(CanvasResourceProviderTest, CanvasResourceProviderTexture) {
  const IntSize kSize(10, 10);
  const CanvasColorParams kColorParams(
      CanvasColorSpace::kSRGB, CanvasColorParams::GetNativeCanvasPixelFormat(),
      kNonOpaque);

  auto provider = CanvasResourceProvider::CreateSharedImageProvider(
      kSize, context_provider_wrapper_, kLow_SkFilterQuality, kColorParams,
      true /*is_origin_top_left*/, CanvasResourceProvider::RasterMode::kGPU,
      0u /*shared_image_usage_flags*/);

  EXPECT_EQ(provider->Size(), kSize);
  EXPECT_TRUE(provider->IsValid());
  EXPECT_TRUE(provider->IsAccelerated());
  EXPECT_TRUE(provider->SupportsDirectCompositing());
  EXPECT_FALSE(provider->SupportsSingleBuffering());
  EXPECT_EQ(provider->ColorParams().ColorSpace(), kColorParams.ColorSpace());
  // As it is an CanvasResourceProviderSharedImage and an accelerated canvas, it
  // will internally force it to kRGBA8
  EXPECT_EQ(provider->ColorParams().PixelFormat(), CanvasPixelFormat::kRGBA8);
  EXPECT_EQ(provider->ColorParams().GetOpacityMode(),
            kColorParams.GetOpacityMode());

  EXPECT_FALSE(provider->IsSingleBuffered());
}

TEST_F(CanvasResourceProviderTest, CanvasResourceProviderUnacceleratedOverlay) {
  const IntSize kSize(10, 10);
  const CanvasColorParams kColorParams(
      CanvasColorSpace::kSRGB, CanvasColorParams::GetNativeCanvasPixelFormat(),
      kNonOpaque);

  auto provider = CanvasResourceProvider::Create(
      kSize,
      CanvasResourceProvider::ResourceUsage::kSoftwareCompositedResourceUsage,
      context_provider_wrapper_, 0 /* msaa_sample_count */,
      kLow_SkFilterQuality, kColorParams,
      CanvasResourceProvider::kAllowImageChromiumPresentationMode,
      nullptr /* resource_dispatcher */, true /* is_origin_top_left */);

  EXPECT_EQ(provider->Size(), kSize);
  EXPECT_TRUE(provider->IsValid());
  EXPECT_FALSE(provider->IsAccelerated());
  EXPECT_TRUE(provider->SupportsDirectCompositing());

  // We do not support single buffering for unaccelerated low latency canvas.
  EXPECT_FALSE(provider->SupportsSingleBuffering());

  EXPECT_EQ(provider->ColorParams().ColorSpace(), kColorParams.ColorSpace());
  EXPECT_EQ(provider->ColorParams().PixelFormat(), kColorParams.PixelFormat());
  EXPECT_EQ(provider->ColorParams().GetOpacityMode(),
            kColorParams.GetOpacityMode());

  EXPECT_FALSE(provider->IsSingleBuffered());
}

TEST_F(CanvasResourceProviderTest,
       CanvasResourceProviderSharedImageResourceRecycling) {
  const IntSize kSize(10, 10);
  const CanvasColorParams kColorParams(
      CanvasColorSpace::kSRGB, CanvasColorParams::GetNativeCanvasPixelFormat(),
      kNonOpaque);

  auto provider = CanvasResourceProvider::Create(
      kSize,
      CanvasResourceProvider::ResourceUsage::
          kAcceleratedCompositedResourceUsage,
      context_provider_wrapper_, 0 /* msaa_sample_count */,
      kMedium_SkFilterQuality, kColorParams,
      CanvasResourceProvider::kAllowImageChromiumPresentationMode,
      nullptr /* resource_dispatcher */, true /* is_origin_top_left */);

  EXPECT_EQ(provider->Size(), kSize);
  EXPECT_TRUE(provider->IsValid());
  EXPECT_TRUE(provider->IsAccelerated());
  EXPECT_FALSE(provider->IsSingleBuffered());
  EXPECT_FALSE(provider->SupportsSingleBuffering());
  EXPECT_EQ(provider->ColorParams().ColorSpace(), kColorParams.ColorSpace());
  // As it is an CanvasResourceProviderSharedImage and an accelerated canvas, it
  // will internally force it to kRGBA8
  EXPECT_EQ(provider->ColorParams().PixelFormat(), CanvasPixelFormat::kRGBA8);
  EXPECT_EQ(provider->ColorParams().GetOpacityMode(),
            kColorParams.GetOpacityMode());

  // Same resource and sync token if we query again without updating.
  auto resource = provider->ProduceCanvasResource();
  auto sync_token = resource->GetSyncToken();
  ASSERT_TRUE(resource);
  EXPECT_EQ(resource, provider->ProduceCanvasResource());
  EXPECT_EQ(sync_token, resource->GetSyncToken());

  // Resource updated after draw.
  provider->Canvas()->clear(SK_ColorWHITE);
  auto new_resource = provider->ProduceCanvasResource();
  EXPECT_NE(resource, new_resource);
  EXPECT_NE(sync_token, new_resource->GetSyncToken());

  // Resource recycled.
  viz::TransferableResource transferable_resource;
  std::unique_ptr<viz::SingleReleaseCallback> release_callback;
  ASSERT_TRUE(resource->PrepareTransferableResource(
      &transferable_resource, &release_callback, kUnverifiedSyncToken));
  auto* resource_ptr = resource.get();
  resource = nullptr;
  release_callback->Run(sync_token, false);

  provider->Canvas()->clear(SK_ColorBLACK);
  auto resource_again = provider->ProduceCanvasResource();
  EXPECT_EQ(resource_ptr, resource_again);
  EXPECT_NE(sync_token, resource_again->GetSyncToken());
}

TEST_F(CanvasResourceProviderTest,
       CanvasResourceProviderSharedImageStaticBitmapImage) {
  const IntSize kSize(10, 10);
  const CanvasColorParams kColorParams(
      CanvasColorSpace::kSRGB, CanvasColorParams::GetNativeCanvasPixelFormat(),
      kNonOpaque);

  auto provider = CanvasResourceProvider::Create(
      kSize,
      CanvasResourceProvider::ResourceUsage::
          kAcceleratedCompositedResourceUsage,
      context_provider_wrapper_, 0 /* msaa_sample_count */,
      kMedium_SkFilterQuality, kColorParams,
      CanvasResourceProvider::kAllowImageChromiumPresentationMode,
      nullptr /* resource_dispatcher */, true /* is_origin_top_left */);
  ASSERT_TRUE(provider->IsValid());

  // Same resource returned until the canvas is updated.
  auto image = provider->Snapshot();
  ASSERT_TRUE(image);
  auto new_image = provider->Snapshot();
  EXPECT_EQ(image->GetMailboxHolder().mailbox,
            new_image->GetMailboxHolder().mailbox);
  EXPECT_EQ(provider->ProduceCanvasResource()->GetOrCreateGpuMailbox(
                kOrderingBarrier),
            image->GetMailboxHolder().mailbox);

  // Resource updated after draw.
  provider->Canvas()->clear(SK_ColorWHITE);
  provider->FlushCanvas();
  new_image = provider->Snapshot();
  EXPECT_NE(new_image->GetMailboxHolder().mailbox,
            image->GetMailboxHolder().mailbox);

  // Resource recycled.
  auto original_mailbox = image->GetMailboxHolder().mailbox;
  image.reset();
  provider->Canvas()->clear(SK_ColorBLACK);
  provider->FlushCanvas();
  EXPECT_EQ(original_mailbox, provider->Snapshot()->GetMailboxHolder().mailbox);
}

TEST_F(CanvasResourceProviderTest,
       CanvasResourceProviderSharedImageCopyOnWriteDisabled) {
  auto* fake_context = static_cast<FakeWebGraphicsContext3DProvider*>(
      context_provider_wrapper_->ContextProvider());
  auto caps = fake_context->GetCapabilities();
  caps.disable_2d_canvas_copy_on_write = true;
  fake_context->SetCapabilities(caps);

  const IntSize kSize(10, 10);
  const CanvasColorParams kColorParams(
      CanvasColorSpace::kSRGB, CanvasColorParams::GetNativeCanvasPixelFormat(),
      kNonOpaque);

  auto provider = CanvasResourceProvider::Create(
      kSize,
      CanvasResourceProvider::ResourceUsage::
          kAcceleratedCompositedResourceUsage,
      context_provider_wrapper_, 0 /* msaa_sample_count */,
      kMedium_SkFilterQuality, kColorParams,
      CanvasResourceProvider::kAllowImageChromiumPresentationMode,
      nullptr /* resource_dispatcher */, true /* is_origin_top_left */);
  ASSERT_TRUE(provider->IsValid());

  // Disabling copy-on-write forces a copy each time the resource is queried.
  auto resource = provider->ProduceCanvasResource();
  EXPECT_NE(resource->GetOrCreateGpuMailbox(kOrderingBarrier),
            provider->ProduceCanvasResource()->GetOrCreateGpuMailbox(
                kOrderingBarrier));
}

TEST_F(CanvasResourceProviderTest, CanvasResourceProviderBitmap) {
  const IntSize kSize(10, 10);
  const CanvasColorParams kColorParams(
      CanvasColorSpace::kSRGB, CanvasColorParams::GetNativeCanvasPixelFormat(),
      kNonOpaque);

  auto provider = CanvasResourceProvider::CreateBitmapProvider(
      kSize, kLow_SkFilterQuality, kColorParams);

  EXPECT_EQ(provider->Size(), kSize);
  EXPECT_TRUE(provider->IsValid());
  EXPECT_FALSE(provider->IsAccelerated());
  EXPECT_FALSE(provider->SupportsDirectCompositing());
  EXPECT_FALSE(provider->SupportsSingleBuffering());
  EXPECT_EQ(provider->ColorParams().ColorSpace(), kColorParams.ColorSpace());
  EXPECT_EQ(provider->ColorParams().PixelFormat(), kColorParams.PixelFormat());
  EXPECT_EQ(provider->ColorParams().GetOpacityMode(),
            kColorParams.GetOpacityMode());

  EXPECT_FALSE(provider->IsSingleBuffered());
}

TEST_F(CanvasResourceProviderTest, CanvasResourceProviderSharedBitmap) {
  const IntSize kSize(10, 10);
  const CanvasColorParams kColorParams(
      CanvasColorSpace::kSRGB, CanvasColorParams::GetNativeCanvasPixelFormat(),
      kNonOpaque);

  MockCanvasResourceDispatcherClient client;
  CanvasResourceDispatcher resource_dispatcher(
      &client, 1 /* client_id */, 1 /* sink_id */,
      1 /* placeholder_canvas_id */, kSize);

  auto provider = CanvasResourceProvider::Create(
      kSize,
      CanvasResourceProvider::ResourceUsage::kSoftwareCompositedResourceUsage,
      context_provider_wrapper_, 0 /* msaa_sample_count */,
      kLow_SkFilterQuality, kColorParams,
      CanvasResourceProvider::kDefaultPresentationMode,
      resource_dispatcher.GetWeakPtr(), true /* is_origin_top_left */);

  EXPECT_EQ(provider->Size(), kSize);
  EXPECT_TRUE(provider->IsValid());
  EXPECT_FALSE(provider->IsAccelerated());
  EXPECT_TRUE(provider->SupportsDirectCompositing());
  EXPECT_FALSE(provider->SupportsSingleBuffering());
  EXPECT_EQ(provider->ColorParams().ColorSpace(), kColorParams.ColorSpace());
  EXPECT_EQ(provider->ColorParams().PixelFormat(), kColorParams.PixelFormat());
  EXPECT_EQ(provider->ColorParams().GetOpacityMode(),
            kColorParams.GetOpacityMode());

  EXPECT_FALSE(provider->IsSingleBuffered());
  provider->TryEnableSingleBuffering();
  EXPECT_FALSE(provider->IsSingleBuffered());
}

TEST_F(CanvasResourceProviderTest,
       CanvasResourceProviderDirect2DGpuMemoryBuffer) {
  const IntSize kSize(10, 10);
  const CanvasColorParams kColorParams(
      CanvasColorSpace::kSRGB, CanvasColorParams::GetNativeCanvasPixelFormat(),
      kNonOpaque);

  auto provider = CanvasResourceProvider::Create(
      kSize,
      CanvasResourceProvider::ResourceUsage::kAcceleratedDirect2DResourceUsage,
      context_provider_wrapper_, 0 /* msaa_sample_count */,
      kLow_SkFilterQuality, kColorParams,
      CanvasResourceProvider::kAllowImageChromiumPresentationMode,
      nullptr /* resource_dispatcher */, true /* is_origin_top_left */);

  EXPECT_EQ(provider->Size(), kSize);
  EXPECT_TRUE(provider->IsValid());
  EXPECT_TRUE(provider->IsAccelerated());
  EXPECT_TRUE(provider->SupportsDirectCompositing());
  EXPECT_TRUE(provider->SupportsSingleBuffering());
  EXPECT_EQ(provider->ColorParams().ColorSpace(), kColorParams.ColorSpace());
  // As it is an CanvasResourceProviderSharedImage and an accelerated canvas, it
  // will internally force it to kRGBA8
  EXPECT_EQ(provider->ColorParams().PixelFormat(), CanvasPixelFormat::kRGBA8);
  EXPECT_EQ(provider->ColorParams().GetOpacityMode(),
            kColorParams.GetOpacityMode());

  EXPECT_FALSE(provider->IsSingleBuffered());
  provider->TryEnableSingleBuffering();
  EXPECT_TRUE(provider->IsSingleBuffered());
}

TEST_F(CanvasResourceProviderTest,
       CanvasResourceProviderDirect3DGpuMemoryBuffer) {
  const IntSize kSize(10, 10);
  const CanvasColorParams kColorParams(
      CanvasColorSpace::kSRGB, CanvasColorParams::GetNativeCanvasPixelFormat(),
      kNonOpaque);

  auto provider = CanvasResourceProvider::Create(
      kSize,
      CanvasResourceProvider::ResourceUsage::kAcceleratedDirect3DResourceUsage,
      context_provider_wrapper_, 0 /* msaa_sample_count */,
      kLow_SkFilterQuality, kColorParams,
      CanvasResourceProvider::kAllowImageChromiumPresentationMode,
      nullptr /* resource_dispatcher */, true /* is_origin_top_left */);

  EXPECT_EQ(provider->Size(), kSize);
  EXPECT_TRUE(provider->IsValid());
  EXPECT_TRUE(provider->IsAccelerated());
  EXPECT_TRUE(provider->SupportsDirectCompositing());
  EXPECT_TRUE(provider->SupportsSingleBuffering());
  EXPECT_EQ(provider->ColorParams().ColorSpace(), kColorParams.ColorSpace());
  EXPECT_EQ(provider->ColorParams().PixelFormat(), kColorParams.PixelFormat());
  EXPECT_EQ(provider->ColorParams().GetOpacityMode(),
            kColorParams.GetOpacityMode());

  EXPECT_FALSE(provider->IsSingleBuffered());
  provider->TryEnableSingleBuffering();
  EXPECT_TRUE(provider->IsSingleBuffered());

  gpu::Mailbox mailbox = gpu::Mailbox::Generate();
  scoped_refptr<ExternalCanvasResource> resource =
      ExternalCanvasResource::Create(
          mailbox, kSize, GL_TEXTURE_2D, kColorParams,
          SharedGpuContext::ContextProviderWrapper(), provider->CreateWeakPtr(),
          kMedium_SkFilterQuality, true /*is_origin_top_left*/);

  // NewOrRecycledResource() would return nullptr before an ImportResource().
  EXPECT_TRUE(provider->ImportResource(resource));
  EXPECT_EQ(provider->NewOrRecycledResource(), resource);
  // NewOrRecycledResource() will always return the same |resource|.
  EXPECT_EQ(provider->NewOrRecycledResource(), resource);
}

// Verifies that Accelerated Direct 3D resources are backed by SharedImages.
// https://crbug.com/985366
TEST_F(CanvasResourceProviderTest, CanvasResourceProviderDirect3DTexture) {
  const IntSize kSize(10, 10);
  const CanvasColorParams kColorParams(
      CanvasColorSpace::kSRGB, CanvasColorParams::GetNativeCanvasPixelFormat(),
      kNonOpaque);

  auto provider = CanvasResourceProvider::Create(
      kSize,
      CanvasResourceProvider::ResourceUsage::kAcceleratedDirect3DResourceUsage,
      context_provider_wrapper_, 0 /* msaa_sample_count */,
      kLow_SkFilterQuality, kColorParams,
      CanvasResourceProvider::kDefaultPresentationMode,
      nullptr /* resource_dispatcher */, true /* is_origin_top_left */);

  EXPECT_EQ(provider->Size(), kSize);
  EXPECT_TRUE(provider->IsValid());
  EXPECT_TRUE(provider->IsAccelerated());
  EXPECT_TRUE(provider->SupportsDirectCompositing());
  EXPECT_FALSE(provider->SupportsSingleBuffering());
  EXPECT_EQ(provider->ColorParams().ColorSpace(), kColorParams.ColorSpace());
  // As it is an CanvasResourceProviderSharedImage and an accelerated canvas, it
  // will internally force it to kRGBA8
  EXPECT_EQ(provider->ColorParams().PixelFormat(), CanvasPixelFormat::kRGBA8);
  EXPECT_EQ(provider->ColorParams().GetOpacityMode(),
            kColorParams.GetOpacityMode());

  EXPECT_FALSE(provider->IsSingleBuffered());
  provider->TryEnableSingleBuffering();
  EXPECT_FALSE(provider->IsSingleBuffered());

  auto resource = provider->ProduceCanvasResource();
  viz::TransferableResource transferable_resource;
  std::unique_ptr<viz::SingleReleaseCallback> callback;
  resource->PrepareTransferableResource(&transferable_resource, &callback,
                                        kOrderingBarrier);
  EXPECT_TRUE(transferable_resource.mailbox_holder.mailbox.IsSharedImage());
  EXPECT_FALSE(transferable_resource.is_overlay_candidate);
  callback->Run(gpu::SyncToken(), true /* is_lost */);
}

TEST_F(CanvasResourceProviderTest, DimensionsExceedMaxTextureSize_Bitmap) {
  const CanvasColorParams kColorParams(
      CanvasColorSpace::kSRGB, CanvasColorParams::GetNativeCanvasPixelFormat(),
      kNonOpaque);

  auto provider = CanvasResourceProvider::CreateBitmapProvider(
      IntSize(kMaxTextureSize - 1, kMaxTextureSize), kLow_SkFilterQuality,
      kColorParams);
  EXPECT_FALSE(provider->SupportsDirectCompositing());
  provider = CanvasResourceProvider::CreateBitmapProvider(
      IntSize(kMaxTextureSize, kMaxTextureSize), kLow_SkFilterQuality,
      kColorParams);
  EXPECT_FALSE(provider->SupportsDirectCompositing());
  provider = CanvasResourceProvider::CreateBitmapProvider(
      IntSize(kMaxTextureSize + 1, kMaxTextureSize), kLow_SkFilterQuality,
      kColorParams);
  EXPECT_FALSE(provider->SupportsDirectCompositing());
}

TEST_F(CanvasResourceProviderTest, DimensionsExceedMaxTextureSize_SharedImage) {
  const CanvasColorParams kColorParams(
      CanvasColorSpace::kSRGB, CanvasColorParams::GetNativeCanvasPixelFormat(),
      kNonOpaque);

  auto provider = CanvasResourceProvider::CreateSharedImageProvider(
      IntSize(kMaxTextureSize - 1, kMaxTextureSize), context_provider_wrapper_,
      kLow_SkFilterQuality, kColorParams, true /*is_origin_top_left*/,
      CanvasResourceProvider::RasterMode::kGPU,
      0u /*shared_image_usage_flags*/);
  EXPECT_TRUE(provider->SupportsDirectCompositing());
  provider = CanvasResourceProvider::CreateSharedImageProvider(
      IntSize(kMaxTextureSize, kMaxTextureSize), context_provider_wrapper_,
      kLow_SkFilterQuality, kColorParams, true /*is_origin_top_left*/,
      CanvasResourceProvider::RasterMode::kGPU,
      0u /*shared_image_usage_flags*/);
  EXPECT_TRUE(provider->SupportsDirectCompositing());
  provider = CanvasResourceProvider::CreateSharedImageProvider(
      IntSize(kMaxTextureSize + 1, kMaxTextureSize), context_provider_wrapper_,
      kLow_SkFilterQuality, kColorParams, true /*is_origin_top_left*/,
      CanvasResourceProvider::RasterMode::kGPU,
      0u /*shared_image_usage_flags*/);
  // The CanvasResourceProvider for SharedImage should not be created or valid
  // if the texture size is greater than the maximum value
  EXPECT_TRUE(!provider || !provider->IsValid());
}

TEST_F(CanvasResourceProviderTest, DimensionsExceedMaxTextureSize) {
  const CanvasColorParams kColorParams(
      CanvasColorSpace::kSRGB, CanvasColorParams::GetNativeCanvasPixelFormat(),
      kNonOpaque);

  for (int i = 0;
       i < static_cast<int>(CanvasResourceProvider::ResourceUsage::kMaxValue);
       ++i) {
    SCOPED_TRACE(i);
    auto usage = static_cast<CanvasResourceProvider::ResourceUsage>(i);
    bool should_support_compositing = false;
    std::unique_ptr<CanvasResourceProvider> provider;
    switch (usage) {
      // Skipping ResourceUsages that will be removed after this refactor
      // bug(1035589)
      case CanvasResourceProvider::ResourceUsage::kSoftwareResourceUsage:
        continue;
      case CanvasResourceProvider::ResourceUsage::kAcceleratedResourceUsage:
        continue;
      case CanvasResourceProvider::ResourceUsage::
          kSoftwareCompositedResourceUsage:
        FALLTHROUGH;
      case CanvasResourceProvider::ResourceUsage::
          kSoftwareCompositedDirect2DResourceUsage:
        FALLTHROUGH;
      case CanvasResourceProvider::ResourceUsage::
          kAcceleratedCompositedResourceUsage:
        FALLTHROUGH;
      case CanvasResourceProvider::ResourceUsage::
          kAcceleratedDirect2DResourceUsage:
        FALLTHROUGH;
      case CanvasResourceProvider::ResourceUsage::
          kAcceleratedDirect3DResourceUsage:
        should_support_compositing = true;
        break;
    }

      provider = CanvasResourceProvider::Create(
          IntSize(kMaxTextureSize - 1, kMaxTextureSize), usage,
          context_provider_wrapper_, 0 /* msaa_sample_count */,
          kLow_SkFilterQuality, kColorParams,
          CanvasResourceProvider::kAllowImageChromiumPresentationMode,
          nullptr /* resource_dispatcher */, true /* is_origin_top_left */);

    EXPECT_EQ(provider->SupportsDirectCompositing(),
              should_support_compositing);

      provider = CanvasResourceProvider::Create(
          IntSize(kMaxTextureSize, kMaxTextureSize), usage,
          context_provider_wrapper_, 0 /* msaa_sample_count */,
          kLow_SkFilterQuality, kColorParams,
          CanvasResourceProvider::kAllowImageChromiumPresentationMode,
          nullptr /* resource_dispatcher */, true /* is_origin_top_left */);

    EXPECT_EQ(provider->SupportsDirectCompositing(),
              should_support_compositing);

      provider = CanvasResourceProvider::Create(
          IntSize(kMaxTextureSize + 1, kMaxTextureSize), usage,
          context_provider_wrapper_, 0 /* msaa_sample_count */,
          kLow_SkFilterQuality, kColorParams,
          CanvasResourceProvider::kAllowImageChromiumPresentationMode,
          nullptr /* resource_dispatcher */, true /* is_origin_top_left */);

    EXPECT_FALSE(provider->SupportsDirectCompositing());
  }
}

TEST_F(CanvasResourceProviderTest, CanvasResourceProviderDirect2DSwapChain) {
  const IntSize kSize(10, 10);
  const CanvasColorParams kColorParams(
      CanvasColorSpace::kSRGB, CanvasColorParams::GetNativeCanvasPixelFormat(),
      kNonOpaque);

  auto provider = CanvasResourceProvider::Create(
      kSize,
      CanvasResourceProvider::ResourceUsage::kAcceleratedDirect2DResourceUsage,
      context_provider_wrapper_, 0 /* msaa_sample_count */,
      kLow_SkFilterQuality, kColorParams,
      CanvasResourceProvider::kAllowSwapChainPresentationMode,
      nullptr /* resource_dispatcher */, true /* is_origin_top_left */);

  EXPECT_EQ(provider->Size(), kSize);
  EXPECT_TRUE(provider->IsValid());
  EXPECT_TRUE(provider->IsAccelerated());
  EXPECT_TRUE(provider->SupportsDirectCompositing());
  EXPECT_TRUE(provider->SupportsSingleBuffering());
  EXPECT_EQ(provider->ColorParams().ColorSpace(), kColorParams.ColorSpace());
  EXPECT_EQ(provider->ColorParams().PixelFormat(), kColorParams.PixelFormat());
  EXPECT_EQ(provider->ColorParams().GetOpacityMode(),
            kColorParams.GetOpacityMode());

  EXPECT_FALSE(provider->IsSingleBuffered());
  provider->TryEnableSingleBuffering();
  EXPECT_TRUE(provider->IsSingleBuffered());
}

}  // namespace blink
