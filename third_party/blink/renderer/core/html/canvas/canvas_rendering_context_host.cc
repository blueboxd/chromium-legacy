// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/canvas/canvas_rendering_context_host.h"

#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_image_encode_options.h"
#include "third_party/blink/renderer/core/html/canvas/canvas_async_blob_creator.h"
#include "third_party/blink/renderer/core/html/canvas/canvas_rendering_context.h"
#include "third_party/blink/renderer/platform/graphics/canvas_2d_layer_bridge.h"
#include "third_party/blink/renderer/platform/graphics/gpu/shared_gpu_context.h"
#include "third_party/blink/renderer/platform/graphics/skia/skia_utils.h"
#include "third_party/blink/renderer/platform/graphics/static_bitmap_image.h"
#include "third_party/blink/renderer/platform/graphics/unaccelerated_static_bitmap_image.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/skia/include/core/SkSurface.h"

namespace blink {

CanvasRenderingContextHost::CanvasRenderingContextHost(HostType host_type)
    : host_type_(host_type) {}

void CanvasRenderingContextHost::RecordCanvasSizeToUMA(const IntSize& size) {
  if (did_record_canvas_size_to_uma_)
    return;
  did_record_canvas_size_to_uma_ = true;

  if (host_type_ == kCanvasHost) {
    UMA_HISTOGRAM_CUSTOM_COUNTS("Blink.Canvas.SqrtNumberOfPixels",
                                std::sqrt(size.Area()), 1, 5000, 100);
  } else if (host_type_ == kOffscreenCanvasHost) {
    UMA_HISTOGRAM_CUSTOM_COUNTS("Blink.OffscreenCanvas.SqrtNumberOfPixels",
                                std::sqrt(size.Area()), 1, 5000, 100);
  } else {
    NOTREACHED();
  }
}

scoped_refptr<StaticBitmapImage>
CanvasRenderingContextHost::CreateTransparentImage(const IntSize& size) const {
  if (!IsValidImageSize(size))
    return nullptr;
  CanvasColorParams color_params = CanvasColorParams();
  if (RenderingContext())
    color_params = RenderingContext()->ColorParams();
  SkImageInfo info = SkImageInfo::Make(
      size.Width(), size.Height(), color_params.GetSkColorType(),
      kPremul_SkAlphaType, color_params.GetSkColorSpaceForSkSurfaces());
  sk_sp<SkSurface> surface =
      SkSurface::MakeRaster(info, info.minRowBytes(), nullptr);
  if (!surface)
    return nullptr;
  return UnacceleratedStaticBitmapImage::Create(surface->makeImageSnapshot());
}

void CanvasRenderingContextHost::Commit(scoped_refptr<CanvasResource>,
                                        const SkIRect&) {
  NOTIMPLEMENTED();
}

bool CanvasRenderingContextHost::IsPaintable() const {
  return (RenderingContext() && RenderingContext()->IsPaintable()) ||
         IsValidImageSize(Size());
}

void CanvasRenderingContextHost::RestoreCanvasMatrixClipStack(
    cc::PaintCanvas* canvas) const {
  if (RenderingContext())
    RenderingContext()->RestoreCanvasMatrixClipStack(canvas);
}

bool CanvasRenderingContextHost::Is3d() const {
  return RenderingContext() && RenderingContext()->Is3d();
}

bool CanvasRenderingContextHost::Is2d() const {
  return RenderingContext() && RenderingContext()->Is2d();
}

CanvasResourceProvider*
CanvasRenderingContextHost::GetOrCreateCanvasResourceProvider(
    AccelerationHint hint) {
  return GetOrCreateCanvasResourceProviderImpl(hint);
}

CanvasResourceProvider*
CanvasRenderingContextHost::GetOrCreateCanvasResourceProviderImpl(
    AccelerationHint hint) {
  if (!ResourceProvider() && !did_fail_to_create_resource_provider_) {
    if (IsValidImageSize(Size())) {
      if (Is3d()) {
        CreateCanvasResourceProvider3D(hint);
      } else {
        CreateCanvasResourceProvider2D(hint);
      }
    }
    if (!ResourceProvider())
      did_fail_to_create_resource_provider_ = true;
  }
  return ResourceProvider();
}

void CanvasRenderingContextHost::CreateCanvasResourceProvider3D(
    AccelerationHint hint) {
  DCHECK(Is3d());

  std::unique_ptr<CanvasResourceProvider> provider;
  base::WeakPtr<CanvasResourceDispatcher> dispatcher =
      GetOrCreateResourceDispatcher()
          ? GetOrCreateResourceDispatcher()->GetWeakPtr()
          : nullptr;

  uint8_t presentation_mode = CanvasResourceProvider::kDefaultPresentationMode;
  if (RenderingContext() && RenderingContext()->UsingSwapChain()) {
    DCHECK(LowLatencyEnabled());
    // Allow swap chain presentation only if 3d context is using a swap
    // chain since we'll be importing it as a passthrough texture.
    presentation_mode |=
        CanvasResourceProvider::kAllowSwapChainPresentationMode;
  }

  if (SharedGpuContext::IsGpuCompositingEnabled() && LowLatencyEnabled()) {
    if (RuntimeEnabledFeatures::WebGLImageChromiumEnabled()) {
      presentation_mode |=
          CanvasResourceProvider::kAllowImageChromiumPresentationMode;
    }

    provider = CanvasResourceProvider::Create(
        Size(),
        CanvasResourceProvider::ResourceUsage::
            kAcceleratedDirect3DResourceUsage,
        SharedGpuContext::ContextProviderWrapper(), 0 /* msaa_sample_count */,
        FilterQuality(), ColorParams(), presentation_mode,
        std::move(dispatcher), RenderingContext()->IsOriginTopLeft());
  } else if (SharedGpuContext::IsGpuCompositingEnabled()) {
    uint32_t shared_image_usage_flags = gpu::SHARED_IMAGE_USAGE_DISPLAY;
    if (RuntimeEnabledFeatures::WebGLImageChromiumEnabled()) {
      shared_image_usage_flags |= gpu::SHARED_IMAGE_USAGE_SCANOUT;
    }
    provider = CanvasResourceProvider::CreateSharedImageProvider(
        Size(), SharedGpuContext::ContextProviderWrapper(), FilterQuality(),
        ColorParams(), RenderingContext()->IsOriginTopLeft(),
        CanvasResourceProvider::RasterMode::kGPU, shared_image_usage_flags);
  }
  if (!provider) {
    provider = CanvasResourceProvider::CreateSharedBitmapProvider(
        Size(), SharedGpuContext::ContextProviderWrapper(), FilterQuality(),
        ColorParams(), std::move(dispatcher));
    if (!provider) {
      provider = CanvasResourceProvider::CreateBitmapProvider(
          Size(), FilterQuality(), ColorParams());
    }
  }

  ReplaceResourceProvider(std::move(provider));
  if (ResourceProvider() && ResourceProvider()->IsValid()) {
    base::UmaHistogramBoolean("Blink.Canvas.ResourceProviderIsAccelerated",
                              ResourceProvider()->IsAccelerated());
    base::UmaHistogramEnumeration("Blink.Canvas.ResourceProviderType",
                                  ResourceProvider()->GetType());
  }
}

void CanvasRenderingContextHost::CreateCanvasResourceProvider2D(
    AccelerationHint hint) {
  DCHECK(Is2d());
  base::WeakPtr<CanvasResourceDispatcher> dispatcher =
      GetOrCreateResourceDispatcher()
          ? GetOrCreateResourceDispatcher()->GetWeakPtr()
          : nullptr;

  std::unique_ptr<CanvasResourceProvider> provider;
  const bool use_gpu =
      hint == kPreferAcceleration && ShouldAccelerate2dContext();
  // It is important to not use the context's IsOriginTopLeft() here
  // because that denotes the current state and could change after the
  // new resource provider is created e.g. due to switching between
  // unaccelerated and accelerated modes during tab switching.
  const bool is_origin_top_left = !use_gpu || LowLatencyEnabled();
  if (use_gpu && LowLatencyEnabled()) {
    // If we can use the gpu and low latency is enabled, we will try to use a
    // SwapChain if possible.
    if (base::FeatureList::IsEnabled(features::kLowLatencyCanvas2dSwapChain)) {
      provider = CanvasResourceProvider::CreateSwapChainProvider(
          Size(), SharedGpuContext::ContextProviderWrapper(), FilterQuality(),
          ColorParams(), is_origin_top_left, std::move(dispatcher),
          GetMSAASampleCountFor2dContext());
    }
    // If SwapChain failed or it was not possible, we will try a SharedImage
    // with a set of flags trying to add Usage Display and Usage Scanout and
    // Concurrent Read and Write if possible.
    if (!provider) {
      uint32_t shared_image_usage_flags = gpu::SHARED_IMAGE_USAGE_DISPLAY;
      if (RuntimeEnabledFeatures::Canvas2dImageChromiumEnabled() ||
          base::FeatureList::IsEnabled(
              features::kLowLatencyCanvas2dImageChromium)) {
        shared_image_usage_flags |= gpu::SHARED_IMAGE_USAGE_SCANOUT;
        shared_image_usage_flags |=
            gpu::SHARED_IMAGE_USAGE_CONCURRENT_READ_WRITE;
      }
      provider = CanvasResourceProvider::CreateSharedImageProvider(
          Size(), SharedGpuContext::ContextProviderWrapper(), FilterQuality(),
          ColorParams(), is_origin_top_left,
          CanvasResourceProvider::RasterMode::kGPU, shared_image_usage_flags);
    }
  } else if (use_gpu) {
    // First try to be optimized for displaying on screen. In the case we are
    // hardware compositing, we also try to enable the usage of the image as
    // scanout buffer (overlay)
    uint32_t shared_image_usage_flags = gpu::SHARED_IMAGE_USAGE_DISPLAY;
    if (RuntimeEnabledFeatures::Canvas2dImageChromiumEnabled())
      shared_image_usage_flags |= gpu::SHARED_IMAGE_USAGE_SCANOUT;
    provider = CanvasResourceProvider::CreateSharedImageProvider(
        Size(), SharedGpuContext::ContextProviderWrapper(), FilterQuality(),
        ColorParams(), is_origin_top_left,
        CanvasResourceProvider::RasterMode::kGPU, shared_image_usage_flags);
  } else if (RuntimeEnabledFeatures::Canvas2dImageChromiumEnabled()) {
    const uint32_t shared_image_usage_flags =
        gpu::SHARED_IMAGE_USAGE_DISPLAY | gpu::SHARED_IMAGE_USAGE_SCANOUT;
    provider = CanvasResourceProvider::CreateSharedImageProvider(
        Size(), SharedGpuContext::ContextProviderWrapper(), FilterQuality(),
        ColorParams(), is_origin_top_left,
        CanvasResourceProvider::RasterMode::kCPU, shared_image_usage_flags);
  }

  // If either of the other modes failed and / or it was not possible to do, we
  // will backup with a SharedBitmap, and if that was not possible with a Bitmap
  // provider.
  if (!provider) {
    provider = CanvasResourceProvider::CreateSharedBitmapProvider(
        Size(), SharedGpuContext::ContextProviderWrapper(), FilterQuality(),
        ColorParams(), std::move(dispatcher));
  }
  if (!provider) {
    provider = CanvasResourceProvider::CreateBitmapProvider(
        Size(), FilterQuality(), ColorParams());
  }

  ReplaceResourceProvider(std::move(provider));

  if (ResourceProvider()) {
    if (ResourceProvider()->IsValid()) {
      base::UmaHistogramBoolean("Blink.Canvas.ResourceProviderIsAccelerated",
                                ResourceProvider()->IsAccelerated());
      base::UmaHistogramEnumeration("Blink.Canvas.ResourceProviderType",
                                    ResourceProvider()->GetType());
    }
    ResourceProvider()->SetFilterQuality(FilterQuality());
    ResourceProvider()->SetResourceRecyclingEnabled(true);
  }
}

CanvasColorParams CanvasRenderingContextHost::ColorParams() const {
  if (RenderingContext())
    return RenderingContext()->ColorParams();
  return CanvasColorParams();
}

ScriptPromise CanvasRenderingContextHost::convertToBlob(
    ScriptState* script_state,
    const ImageEncodeOptions* options,
    ExceptionState& exception_state) {
  WTF::String object_name = "Canvas";
  if (this->IsOffscreenCanvas())
    object_name = "OffscreenCanvas";
  std::stringstream error_msg;

  if (this->IsOffscreenCanvas() && this->IsNeutered()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "OffscreenCanvas object is detached.");
    return ScriptPromise();
  }

  if (!this->OriginClean()) {
    error_msg << "Tainted " << object_name << " may not be exported.";
    exception_state.ThrowSecurityError(error_msg.str().c_str());
    return ScriptPromise();
  }

  // It's possible that there are recorded commands that have not been resolved
  // Finalize frame will be called in GetImage, but if there's no
  // resourceProvider yet then the IsPaintable check will fail
  if (RenderingContext())
    RenderingContext()->FinalizeFrame();

  if (!this->IsPaintable() || Size().IsEmpty()) {
    error_msg << "The size of " << object_name << " is zero.";
    exception_state.ThrowDOMException(DOMExceptionCode::kIndexSizeError,
                                      error_msg.str().c_str());
    return ScriptPromise();
  }

  if (!RenderingContext()) {
    error_msg << object_name << " has no rendering context.";
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      error_msg.str().c_str());
    return ScriptPromise();
  }

  base::TimeTicks start_time = base::TimeTicks::Now();
  scoped_refptr<StaticBitmapImage> image_bitmap =
      RenderingContext()->GetImage(kPreferNoAcceleration);
  if (image_bitmap) {
    auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
    CanvasAsyncBlobCreator::ToBlobFunctionType function_type =
        CanvasAsyncBlobCreator::kHTMLCanvasConvertToBlobPromise;
    if (this->IsOffscreenCanvas()) {
      function_type =
          CanvasAsyncBlobCreator::kOffscreenCanvasConvertToBlobPromise;
    }
    auto* async_creator = MakeGarbageCollected<CanvasAsyncBlobCreator>(
        image_bitmap, options, function_type, start_time,
        ExecutionContext::From(script_state), resolver);
    async_creator->ScheduleAsyncBlobCreation(options->quality());
    return resolver->Promise();
  }
  exception_state.ThrowDOMException(DOMExceptionCode::kNotReadableError,
                                    "Readback of the source image has failed.");
  return ScriptPromise();
}

bool CanvasRenderingContextHost::IsOffscreenCanvas() const {
  return host_type_ == kOffscreenCanvasHost;
}

}  // namespace blink
