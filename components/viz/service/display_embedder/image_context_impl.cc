// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display_embedder/image_context_impl.h"

#include <utility>

#include "components/viz/common/resources/resource_format_utils.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/service/mailbox_manager.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "gpu/command_buffer/service/shared_image_factory.h"
#include "gpu/command_buffer/service/shared_image_representation.h"
#include "gpu/command_buffer/service/skia_utils.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "third_party/skia/include/core/SkPromiseImageTexture.h"

namespace viz {

ImageContextImpl::ImageContextImpl(const gpu::MailboxHolder& mailbox_holder,
                                   const gfx::Size& size,
                                   ResourceFormat resource_format,
                                   sk_sp<SkColorSpace> color_space)
    : ImageContext(mailbox_holder, size, resource_format, color_space) {}

ImageContextImpl::ImageContextImpl(RenderPassId render_pass_id,
                                   const gfx::Size& size,
                                   ResourceFormat resource_format,
                                   bool mipmap,
                                   sk_sp<SkColorSpace> color_space)
    : ImageContext(gpu::MailboxHolder(),
                   size,
                   resource_format,
                   std::move(color_space)),
      render_pass_id_(render_pass_id),
      mipmap_(mipmap ? GrMipMapped::kYes : GrMipMapped::kNo) {}

ImageContextImpl::~ImageContextImpl() {
  DCHECK(!representation_is_being_accessed_);

  if (fallback_context_state_)
    gpu::DeleteGrBackendTexture(fallback_context_state_, &fallback_texture_);
}

void ImageContextImpl::CreateFallbackImage(
    gpu::SharedContextState* context_state) {
  DCHECK(!context_state);
  fallback_context_state_ = context_state;

  SkColorType color_type = ResourceFormatToClosestSkColorType(
      true /* gpu_compositing */, resource_format());

  // Don't use a fallback SkColorType because we may fail checks inside Skia
  if (SkColorTypeBytesPerPixel(color_type) * 8 !=
      BitsPerPixel(resource_format()))
    return;

  fallback_texture_ =
      fallback_context_state_->gr_context()->createBackendTexture(
          size().width(), size().height(), color_type,
#if DCHECK_IS_ON()
          SkColors::kRed,
#else
          SkColors::kWhite,
#endif
          GrMipMapped::kNo, GrRenderable::kYes);

  if (!fallback_texture_.isValid()) {
    DLOG(ERROR) << "Could not create backend texture.";
    return;
  }
  promise_image_texture_ = SkPromiseImageTexture::Make(fallback_texture_);
}

void ImageContextImpl::BeginAccess(
    gpu::SharedContextState* context_state,
    gpu::SharedImageRepresentationFactory* representation_factory,
    gpu::MailboxManager* mailbox_manager,
    const gl::GLVersionInfo* gl_version_info,
    std::vector<GrBackendSemaphore>* begin_semaphores,
    std::vector<GrBackendSemaphore>* end_semaphores) {
  // Prepare for accessing shared image.
  if (mailbox_holder().mailbox.IsSharedImage()) {
    if (promise_image_texture_) {
      // The promise image has been fulfilled early, so we just need begin
      // access the shared image representation. If representation is nullptr,
      // it means the promise image was not fulfilled successfully last time
      // and a fallback content has been used, in that case, we cannot change
      // the content, so we do nothing.
      if (representation_) {
        auto promise_image_texture =
            representation_->BeginReadAccess(begin_semaphores, end_semaphores);
        // The image has been fulfilled and cached. It is too late to tell
        // skia the backing of the cached image is not accessible right now,
        // so crash for now.
        // TODO(penghuang): find a way to notify skia.
        CHECK(promise_image_texture);
        representation_is_being_accessed_ = true;
      }
      return;
    }

    auto representation = representation_factory->ProduceSkia(
        mailbox_holder().mailbox, context_state);
    if (!representation) {
      DLOG(ERROR) << "Failed to fulfill the promise texture - SharedImage "
                     "mailbox not found in SharedImageManager.";
      CreateFallbackImage(context_state);
      return;
    }

    if (!(representation->usage() & gpu::SHARED_IMAGE_USAGE_DISPLAY)) {
      DLOG(ERROR) << "Failed to fulfill the promise texture - SharedImage "
                     "was not created with display usage.";
      CreateFallbackImage(context_state);
      return;
    }

    if (representation->size() != size()) {
      DLOG(ERROR) << "Failed to fulfill the promise texture - SharedImage "
                     "size does not match TransferableResource size.";
      CreateFallbackImage(context_state);
      return;
    }

    representation_ = std::move(representation);
    promise_image_texture_ =
        representation_->BeginReadAccess(begin_semaphores, end_semaphores);
    if (!promise_image_texture_) {
      DLOG(ERROR) << "Failed to fulfill the promise texture - SharedImage "
                     "begin read access failed..";
      CreateFallbackImage(context_state);
      return;
    }
    representation_is_being_accessed_ = true;
    return;
  }

  // Prepare for accessing legacy mailbox.
  // The promise image has been fulfilled once, so we do need do anything.
  if (promise_image_texture_)
    return;

  if (!context_state->GrContextIsGL()) {
    // Probably this texture is created with wrong interface
    // (GLES2Interface).
    DLOG(ERROR) << "Failed to fulfill the promise texture whose backend is not "
                   "compatible with vulkan.";
    CreateFallbackImage(context_state);
    return;
  }

  auto* texture_base =
      mailbox_manager->ConsumeTexture(mailbox_holder().mailbox);
  if (!texture_base) {
    DLOG(ERROR) << "Failed to fulfill the promise texture.";
    CreateFallbackImage(context_state);
    return;
  }

  gfx::Size texture_size;
  if (BindOrCopyTextureIfNecessary(texture_base, &texture_size) &&
      texture_size != size()) {
    DLOG(ERROR) << "Failed to fulfill the promise texture - texture "
                   "size does not match TransferableResource size.";
    CreateFallbackImage(context_state);
    return;
  }

  GrBackendTexture backend_texture;
  gpu::GetGrBackendTexture(gl_version_info, texture_base->target(), size(),
                           texture_base->service_id(), resource_format(),
                           &backend_texture);
  if (!backend_texture.isValid()) {
    DLOG(ERROR) << "Failed to fulfill the promise texture.";
    CreateFallbackImage(context_state);
    return;
  }
  promise_image_texture_ = SkPromiseImageTexture::Make(backend_texture);
}

bool ImageContextImpl::BindOrCopyTextureIfNecessary(
    gpu::TextureBase* texture_base,
    gfx::Size* size) {
  if (texture_base->GetType() != gpu::TextureBase::Type::kValidated)
    return false;
  // If a texture is validated and bound to an image, we may defer copying the
  // image to the texture until the texture is used. It is for implementing low
  // latency drawing (e.g. fast ink) and avoiding unnecessary texture copy. So
  // we need check the texture image state, and bind or copy the image to the
  // texture if necessary.
  auto* texture = gpu::gles2::Texture::CheckedCast(texture_base);
  gpu::gles2::Texture::ImageState image_state;
  auto* image = texture->GetLevelImage(GL_TEXTURE_2D, 0, &image_state);
  if (image && image_state == gpu::gles2::Texture::UNBOUND) {
    glBindTexture(texture_base->target(), texture_base->service_id());
    if (image->ShouldBindOrCopy() == gl::GLImage::BIND) {
      if (!image->BindTexImage(texture_base->target())) {
        LOG(ERROR) << "Failed to bind a gl image to texture.";
        return false;
      }
    } else {
      texture->SetLevelImageState(texture_base->target(), 0,
                                  gpu::gles2::Texture::COPIED);
      if (!image->CopyTexImage(texture_base->target())) {
        LOG(ERROR) << "Failed to copy a gl image to texture.";
        return false;
      }
    }
  }
  GLsizei temp_width, temp_height;
  texture->GetLevelSize(texture_base->target(), 0 /* level */, &temp_width,
                        &temp_height, nullptr /* depth */);
  *size = gfx::Size(temp_width, temp_height);
  return true;
}

void ImageContextImpl::EndAccess() {
  if (!representation_is_being_accessed_)
    return;

  // TODO(penghuang): create end read access semaphores and call
  // EndReadAccess() with them. https://crbug.com/944194
  DCHECK(representation_);
  representation_->EndReadAccess();
  representation_is_being_accessed_ = false;
}

}  // namespace viz
