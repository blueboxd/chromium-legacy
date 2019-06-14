// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_IMAGE_CONTEXT_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_IMAGE_CONTEXT_H_

#include <memory>

#include "base/macros.h"
#include "components/viz/common/quads/render_pass.h"
#include "components/viz/common/resources/resource_format.h"
#include "components/viz/service/display/resource_metadata.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "third_party/skia/include/gpu/GrBackendSurface.h"
#include "third_party/skia/include/gpu/GrTypes.h"
#include "ui/gfx/geometry/size.h"

class SkColorSpace;
class SkPromiseImageTexture;

namespace gpu {
class SharedImageRepresentationSkia;
}

namespace viz {

// Contex for hold promise image related properties.
struct ImageContext {
  ImageContext(const gpu::Mailbox& mailbox,
               const gfx::Size& size,
               ResourceFormat resource_format,
               sk_sp<SkColorSpace> color_space,
               SkAlphaType alpha_type,
               GrSurfaceOrigin origin,
               const gpu::SyncToken& sync_token);
  explicit ImageContext(const ResourceMetadata& metadata);
  ImageContext(RenderPassId render_pass_id,
               const gfx::Size& size,
               ResourceFormat resource_format,
               bool mipmap,
               sk_sp<SkColorSpace> color_space);
  ~ImageContext();

  // Properties for promise images which are created from a resource.
  const gpu::Mailbox mailbox;

  // Properties for promise images which are created from a render pass.
  const RenderPassId render_pass_id = 0;

  // Const properties which can be accessed by display and GPU threads.
  const gfx::Size size;
  const ResourceFormat resource_format;
  const GrMipMapped mipmap = GrMipMapped::kNo;
  const sk_sp<SkColorSpace> color_space;
  const SkAlphaType alpha_type = kPremul_SkAlphaType;
  const GrSurfaceOrigin origin = kTopLeft_GrSurfaceOrigin;

  // The promise image which is used on display thread.
  sk_sp<SkImage> image;

  // Fallback in case we cannot produce a |representation|.
  GrBackendTexture fallback_texture;

  // |sync_token| is only accessed on display thread.
  gpu::SyncToken sync_token;

  // SharedImage |representation| is only accessed on GPU thread.
  std::unique_ptr<gpu::SharedImageRepresentationSkia> representation;

  // |representation_is_being_accessed| is used on GPU thread only.
  bool representation_is_being_accessed = false;

  // The |promise_image_texture| is used for fulfilling the promise image. It is
  // used on GPU thread.
  sk_sp<SkPromiseImageTexture> promise_image_texture;

 private:
  DISALLOW_COPY_AND_ASSIGN(ImageContext);
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_IMAGE_CONTEXT_H_
