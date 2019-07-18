// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/canvas_resource_provider.h"

#include "components/viz/common/resources/single_release_callback.h"
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
#include "third_party/blink/renderer/platform/wtf/functional.h"

using testing::_;
using testing::InSequence;
using testing::Return;
using testing::Test;

namespace blink {

namespace {

class MockCanvasResourceDispatcherClient
    : public CanvasResourceDispatcherClient {
 public:
  MockCanvasResourceDispatcherClient() = default;

  MOCK_METHOD0(BeginFrame, void());
};

class MockWebGraphisContext3DProviderWrapper
    : public WebGraphicsContext3DProvider {
 public:
  MockWebGraphisContext3DProviderWrapper(cc::ImageDecodeCache* cache = nullptr)
      : image_decode_cache_(cache ? cache : &stub_image_decode_cache_) {
    // enable all gpu features.
    for (unsigned feature = 0; feature < gpu::NUMBER_OF_GPU_FEATURE_TYPES;
         ++feature) {
      gpu_feature_info_.status_values[feature] = gpu::kGpuFeatureStatusEnabled;
    }
  }
  ~MockWebGraphisContext3DProviderWrapper() override = default;

  GrContext* GetGrContext() override {
    return GetTestContextProvider()->GrContext();
  }

  const gpu::Capabilities& GetCapabilities() const override {
    return capabilities_;
  }
  void SetCapabilities(const gpu::Capabilities& c) { capabilities_ = c; }

  const gpu::GpuFeatureInfo& GetGpuFeatureInfo() const override {
    return gpu_feature_info_;
  }

  const WebglPreferences& GetWebglPreferences() const override {
    return webgl_preferences_;
  }

  viz::GLHelper* GetGLHelper() override { return nullptr; }

  gpu::gles2::GLES2Interface* ContextGL() override {
    return GetTestContextProvider()->ContextGL();
  }

  gpu::webgpu::WebGPUInterface* WebGPUInterface() override { return nullptr; }

  scoped_refptr<viz::TestContextProvider> GetTestContextProvider() {
    if (!test_context_provider_) {
      test_context_provider_ = viz::TestContextProvider::Create();
      // Needed for CanvasResourceProviderDirect2DGpuMemoryBuffer.
      test_context_provider_->UnboundTestContextGL()
          ->set_support_texture_format_bgra8888(true);
      test_context_provider_->BindToCurrentThread();
    }
    return test_context_provider_;
  }

  bool BindToCurrentThread() override { return false; }
  void SetLostContextCallback(base::RepeatingClosure) override {}
  void SetErrorMessageCallback(
      base::RepeatingCallback<void(const char*, int32_t id)>) override {}
  cc::ImageDecodeCache* ImageDecodeCache(SkColorType color_type) override {
    return image_decode_cache_;
  }
  viz::TestSharedImageInterface* SharedImageInterface() override {
    return GetTestContextProvider()->SharedImageInterface();
  }
  void CopyVideoFrame(media::PaintCanvasVideoRenderer* video_render,
                      media::VideoFrame* video_frame,
                      cc::PaintCanvas* canvas) override {}

 private:
  cc::StubDecodeCache stub_image_decode_cache_;

  scoped_refptr<viz::TestContextProvider> test_context_provider_;
  gpu::Capabilities capabilities_;
  gpu::GpuFeatureInfo gpu_feature_info_;
  WebglPreferences webgl_preferences_;
  cc::ImageDecodeCache* image_decode_cache_;
};

}  // anonymous namespace

class CanvasResourceProviderTest : public Test {
 public:
  void SetUp() override {
    // Install our mock GL context so that it gets served by SharedGpuContext.
    auto factory = [](bool* gpu_compositing_disabled)
        -> std::unique_ptr<WebGraphicsContext3DProvider> {
      *gpu_compositing_disabled = false;
      // Unretained is safe since TearDown() cleans up the SharedGpuContext.
      return std::make_unique<MockWebGraphisContext3DProviderWrapper>();
    };
    SharedGpuContext::SetContextProviderFactoryForTesting(
        WTF::BindRepeating(factory));
    context_provider_wrapper_ = SharedGpuContext::ContextProviderWrapper();
  }

  void TearDown() override { SharedGpuContext::ResetForTesting(); }

  // Adds |buffer_format| to the context capabilities if it's not supported.
  void EnsureBufferFormatIsSupported(gfx::BufferFormat buffer_format) {
    auto* context_provider = context_provider_wrapper_->ContextProvider();
    if (context_provider->GetCapabilities().gpu_memory_buffer_formats.Has(
            buffer_format)) {
      return;
    }

    auto capabilities = context_provider->GetCapabilities();
    capabilities.gpu_memory_buffer_formats.Add(buffer_format);

    static_cast<MockWebGraphisContext3DProviderWrapper*>(context_provider)
        ->SetCapabilities(capabilities);
  }

 protected:
  base::WeakPtr<WebGraphicsContext3DProviderWrapper> context_provider_wrapper_;
  ScopedTestingPlatformSupport<GpuMemoryBufferTestPlatform> platform_;
};

TEST_F(CanvasResourceProviderTest,
       CanvasResourceProviderTextureGpuMemoryBuffer) {
  const IntSize kSize(10, 10);
  const CanvasColorParams kColorParams(kSRGBCanvasColorSpace,
                                       kRGBA8CanvasPixelFormat, kNonOpaque);
  EnsureBufferFormatIsSupported(kColorParams.GetBufferFormat());

  auto provider = CanvasResourceProvider::CreateForTesting(
      kSize, CanvasResourceProvider::kTextureGpuMemoryBuffer,
      context_provider_wrapper_, 0 /* msaa_sample_count */, kColorParams,
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
}

TEST_F(CanvasResourceProviderTest, CanvasResourceProviderTexture) {
  const IntSize kSize(10, 10);
  const CanvasColorParams kColorParams(kSRGBCanvasColorSpace,
                                       kRGBA8CanvasPixelFormat, kNonOpaque);

  auto provider = CanvasResourceProvider::Create(
      kSize, CanvasResourceProvider::ResourceUsage::kAcceleratedResourceUsage,
      context_provider_wrapper_, 0 /* msaa_sample_count */, kColorParams,
      CanvasResourceProvider::kAllowImageChromiumPresentationMode,
      nullptr /* resource_dispatcher */, true /* is_origin_top_left */);

  EXPECT_EQ(provider->Size(), kSize);
  EXPECT_TRUE(provider->IsValid());
  EXPECT_TRUE(provider->IsAccelerated());
  EXPECT_TRUE(provider->SupportsDirectCompositing());
  EXPECT_FALSE(provider->SupportsSingleBuffering());
  EXPECT_EQ(provider->ColorParams().ColorSpace(), kColorParams.ColorSpace());
  EXPECT_EQ(provider->ColorParams().PixelFormat(), kColorParams.PixelFormat());
  EXPECT_EQ(provider->ColorParams().GetOpacityMode(),
            kColorParams.GetOpacityMode());

  EXPECT_FALSE(provider->IsSingleBuffered());
}

TEST_F(CanvasResourceProviderTest,
       CanvasResourceProviderBitmapGpuMemoryBuffer) {
  const IntSize kSize(10, 10);
  const CanvasColorParams kColorParams(kSRGBCanvasColorSpace,
                                       kRGBA8CanvasPixelFormat, kNonOpaque);
  EnsureBufferFormatIsSupported(kColorParams.GetBufferFormat());

  auto provider = CanvasResourceProvider::Create(
      kSize,
      CanvasResourceProvider::ResourceUsage::kSoftwareCompositedResourceUsage,
      context_provider_wrapper_, 0 /* msaa_sample_count */, kColorParams,
      CanvasResourceProvider::kAllowImageChromiumPresentationMode,
      nullptr /* resource_dispatcher */, true /* is_origin_top_left */);

  EXPECT_EQ(provider->Size(), kSize);
  EXPECT_TRUE(provider->IsValid());
  EXPECT_FALSE(provider->IsAccelerated());
  EXPECT_TRUE(provider->SupportsDirectCompositing());
  EXPECT_TRUE(provider->SupportsSingleBuffering());
  EXPECT_EQ(provider->ColorParams().ColorSpace(), kColorParams.ColorSpace());
  EXPECT_EQ(provider->ColorParams().PixelFormat(), kColorParams.PixelFormat());
  EXPECT_EQ(provider->ColorParams().GetOpacityMode(),
            kColorParams.GetOpacityMode());

  EXPECT_FALSE(provider->IsSingleBuffered());
}

TEST_F(CanvasResourceProviderTest, CanvasResourceProviderSharedImage) {
  const IntSize kSize(10, 10);
  const CanvasColorParams kColorParams(kSRGBCanvasColorSpace,
                                       kRGBA8CanvasPixelFormat, kNonOpaque);
  EnsureBufferFormatIsSupported(kColorParams.GetBufferFormat());

  auto provider = CanvasResourceProvider::CreateForTesting(
      kSize, CanvasResourceProvider::kSharedImage, context_provider_wrapper_,
      0 /* msaa_sample_count */, kColorParams,
      CanvasResourceProvider::kAllowImageChromiumPresentationMode,
      nullptr /* resource_dispatcher */, true /* is_origin_top_left */);

  EXPECT_EQ(provider->Size(), kSize);
  EXPECT_TRUE(provider->IsValid());
  EXPECT_TRUE(provider->IsAccelerated());
  EXPECT_FALSE(provider->IsSingleBuffered());
  EXPECT_FALSE(provider->SupportsSingleBuffering());
  EXPECT_EQ(provider->ColorParams().ColorSpace(), kColorParams.ColorSpace());
  EXPECT_EQ(provider->ColorParams().PixelFormat(), kColorParams.PixelFormat());
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
  const CanvasColorParams kColorParams(kSRGBCanvasColorSpace,
                                       kRGBA8CanvasPixelFormat, kNonOpaque);
  EnsureBufferFormatIsSupported(kColorParams.GetBufferFormat());

  auto provider = CanvasResourceProvider::CreateForTesting(
      kSize, CanvasResourceProvider::kSharedImage, context_provider_wrapper_,
      0 /* msaa_sample_count */, kColorParams,
      CanvasResourceProvider::kAllowImageChromiumPresentationMode,
      nullptr /* resource_dispatcher */, true /* is_origin_top_left */);
  ASSERT_TRUE(provider->IsValid());

  // Same resource returned until the canvas is updated.
  auto image = provider->Snapshot();
  ASSERT_TRUE(image);
  auto new_image = provider->Snapshot();
  EXPECT_EQ(image->GetMailbox(), new_image->GetMailbox());
  EXPECT_EQ(provider->ProduceCanvasResource()->GetOrCreateGpuMailbox(
                kOrderingBarrier),
            image->GetMailbox());

  // Resource updated after draw.
  provider->Canvas()->clear(SK_ColorWHITE);
  new_image = provider->Snapshot();
  EXPECT_NE(new_image->GetMailbox(), image->GetMailbox());

  // Resource recycled.
  auto original_mailbox = image->GetMailbox();
  image.reset();
  provider->Canvas()->clear(SK_ColorBLACK);
  EXPECT_EQ(original_mailbox, provider->Snapshot()->GetMailbox());
}

TEST_F(CanvasResourceProviderTest,
       CanvasResourceProviderSharedImageCopyOnWriteDisabled) {
  auto* mock_context = static_cast<MockWebGraphisContext3DProviderWrapper*>(
      context_provider_wrapper_->ContextProvider());
  auto caps = mock_context->GetCapabilities();
  caps.disable_2d_canvas_copy_on_write = true;
  mock_context->SetCapabilities(caps);

  const IntSize kSize(10, 10);
  const CanvasColorParams kColorParams(kSRGBCanvasColorSpace,
                                       kRGBA8CanvasPixelFormat, kNonOpaque);
  EnsureBufferFormatIsSupported(kColorParams.GetBufferFormat());

  auto provider = CanvasResourceProvider::CreateForTesting(
      kSize, CanvasResourceProvider::kSharedImage, context_provider_wrapper_,
      0 /* msaa_sample_count */, kColorParams,
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
  const CanvasColorParams kColorParams(kSRGBCanvasColorSpace,
                                       kRGBA8CanvasPixelFormat, kNonOpaque);

  auto provider = CanvasResourceProvider::Create(
      kSize, CanvasResourceProvider::ResourceUsage::kSoftwareResourceUsage,
      context_provider_wrapper_, 0 /* msaa_sample_count */, kColorParams,
      CanvasResourceProvider::kAllowImageChromiumPresentationMode,
      nullptr /* resource_dispatcher */, true /* is_origin_top_left */);

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
  const CanvasColorParams kColorParams(kSRGBCanvasColorSpace,
                                       kRGBA8CanvasPixelFormat, kNonOpaque);

  MockCanvasResourceDispatcherClient client;
  CanvasResourceDispatcher resource_dispatcher(
      &client, 1 /* client_id */, 1 /* sink_id */,
      1 /* placeholder_canvas_id */, kSize);

  auto provider = CanvasResourceProvider::Create(
      kSize,
      CanvasResourceProvider::ResourceUsage::kSoftwareCompositedResourceUsage,
      context_provider_wrapper_, 0 /* msaa_sample_count */, kColorParams,
      CanvasResourceProvider::kDefaultPresentationMode,
      resource_dispatcher.GetWeakPtr(), true /* is_origin_top_left */);

  EXPECT_EQ(provider->Size(), kSize);
  EXPECT_TRUE(provider->IsValid());
  EXPECT_FALSE(provider->IsAccelerated());
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

TEST_F(CanvasResourceProviderTest,
       CanvasResourceProviderDirect2DGpuMemoryBuffer) {
  const IntSize kSize(10, 10);
  const CanvasColorParams kColorParams(kSRGBCanvasColorSpace,
                                       kRGBA8CanvasPixelFormat, kNonOpaque);
  EnsureBufferFormatIsSupported(kColorParams.GetBufferFormat());

  auto provider = CanvasResourceProvider::Create(
      kSize,
      CanvasResourceProvider::ResourceUsage::kAcceleratedDirect2DResourceUsage,
      context_provider_wrapper_, 0 /* msaa_sample_count */, kColorParams,
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
}

TEST_F(CanvasResourceProviderTest,
       CanvasResourceProviderDirect3DGpuMemoryBuffer) {
  const IntSize kSize(10, 10);
  const CanvasColorParams kColorParams(kSRGBCanvasColorSpace,
                                       kRGBA8CanvasPixelFormat, kNonOpaque);
  EnsureBufferFormatIsSupported(kColorParams.GetBufferFormat());

  auto provider = CanvasResourceProvider::Create(
      kSize,
      CanvasResourceProvider::ResourceUsage::kAcceleratedDirect3DResourceUsage,
      context_provider_wrapper_, 0 /* msaa_sample_count */, kColorParams,
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
          kNone_SkFilterQuality);

  // NewOrRecycledResource() would return nullptr before an ImportResource().
  EXPECT_TRUE(provider->ImportResource(resource));
  EXPECT_EQ(provider->NewOrRecycledResource(), resource);
  // NewOrRecycledResource() will always return the same |resource|.
  EXPECT_EQ(provider->NewOrRecycledResource(), resource);
}

}  // namespace blink
