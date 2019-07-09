// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/android/texture_owner.h"

#include <memory>

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/threading/thread_task_runner_handle.h"
#include "gpu/command_buffer/service/abstract_texture.h"
#include "gpu/command_buffer/service/abstract_texture_impl_shared_context_state.h"
#include "gpu/command_buffer/service/decoder_context.h"
#include "gpu/command_buffer/service/feature_info.h"
#include "gpu/command_buffer/service/texture_base.h"
#include "media/gpu/android/image_reader_gl_owner.h"
#include "media/gpu/android/surface_texture_gl_owner.h"
#include "ui/gl/scoped_binders.h"

namespace media {

TextureOwner::TextureOwner(bool binds_texture_on_update,
                           std::unique_ptr<gpu::gles2::AbstractTexture> texture)
    : base::RefCountedDeleteOnSequence<TextureOwner>(
          base::ThreadTaskRunnerHandle::Get()),
      binds_texture_on_update_(binds_texture_on_update),
      texture_(std::move(texture)),
      task_runner_(base::ThreadTaskRunnerHandle::Get()) {
  // Notify the subclass when the texture is destroyed.
  // Unretained is safe, since we insist that |texture_| is dropped before we're
  // destroyed, which implies that the callback has run.
  texture_->SetCleanupCallback(base::BindOnce(&TextureOwner::OnTextureDestroyed,
                                              base::Unretained(this)));
}

TextureOwner::~TextureOwner() {
  // The subclass must delete the texture before now.
  DCHECK(!texture_);
}

// static
scoped_refptr<TextureOwner> TextureOwner::Create(
    std::unique_ptr<gpu::gles2::AbstractTexture> texture,
    Mode mode) {
  switch (mode) {
    case Mode::kAImageReaderInsecure:
    case Mode::kAImageReaderInsecureSurfaceControl:
    case Mode::kAImageReaderSecureSurfaceControl:
      return new ImageReaderGLOwner(std::move(texture), mode);
    case Mode::kSurfaceTextureInsecure:
      return new SurfaceTextureGLOwner(std::move(texture));
  }

  NOTREACHED();
  return nullptr;
}

// static
std::unique_ptr<gpu::gles2::AbstractTexture> TextureOwner::CreateTexture(
    scoped_refptr<gpu::SharedContextState> context_state) {
  DCHECK(context_state);

  gpu::gles2::FeatureInfo* feature_info = context_state->feature_info();
  if (feature_info && feature_info->is_passthrough_cmd_decoder()) {
    return std::make_unique<
        gpu::gles2::AbstractTextureImplOnSharedContextPassthrough>(
        GL_TEXTURE_EXTERNAL_OES, std::move(context_state));
  }

  return std::make_unique<gpu::gles2::AbstractTextureImplOnSharedContext>(
      GL_TEXTURE_EXTERNAL_OES, GL_RGBA,
      0,  // width
      0,  // height
      1,  // depth
      0,  // border
      GL_RGBA, GL_UNSIGNED_BYTE, std::move(context_state));
}

GLuint TextureOwner::GetTextureId() const {
  return texture_->service_id();
}

gpu::TextureBase* TextureOwner::GetTextureBase() const {
  return texture_->GetTextureBase();
}

void TextureOwner::ClearAbstractTexture() {
  texture_.reset();
}

}  // namespace media
