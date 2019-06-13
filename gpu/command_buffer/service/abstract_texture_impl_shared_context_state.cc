// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/abstract_texture_impl_shared_context_state.h"

#include <utility>

#include "gpu/command_buffer/service/context_state.h"
#include "gpu/command_buffer/service/texture_manager.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/scoped_binders.h"
#include "ui/gl/scoped_make_current.h"

namespace gpu {
namespace gles2 {

AbstractTextureImplOnSharedContext::AbstractTextureImplOnSharedContext(
    GLenum target,
    GLenum internal_format,
    GLsizei width,
    GLsizei height,
    GLsizei depth,
    GLint border,
    GLenum format,
    GLenum type,
    scoped_refptr<gpu::SharedContextState> shared_context_state)
    : shared_context_state_(std::move(shared_context_state)) {
  DCHECK(shared_context_state_);

  // The calling code which wants to create this abstract texture should have
  // already made the shared context current.
  DCHECK(shared_context_state_->IsCurrent(nullptr));

  // Create a gles2 Texture.
  GLuint service_id = 0;
  auto* api = gl::g_current_gl_context;
  api->glGenTexturesFn(1, &service_id);
  gl::ScopedTextureBinder binder(target, service_id);
  api->glTexParameteriFn(target, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  api->glTexParameteriFn(target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  api->glTexParameteriFn(target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  api->glTexParameteriFn(target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

  texture_ = new gpu::gles2::Texture(service_id);
  texture_->SetLightweightRef();
  texture_->SetTarget(target, 1);
  texture_->sampler_state_.min_filter = GL_LINEAR;
  texture_->sampler_state_.mag_filter = GL_LINEAR;
  texture_->sampler_state_.wrap_t = GL_CLAMP_TO_EDGE;
  texture_->sampler_state_.wrap_s = GL_CLAMP_TO_EDGE;
  gfx::Rect cleared_rect;
  texture_->SetLevelInfo(target, 0, internal_format, width, height, depth,
                         border, format, type, cleared_rect);
  texture_->SetImmutable(true);
  shared_context_state_->AddContextLostObserver(this);
}

AbstractTextureImplOnSharedContext::~AbstractTextureImplOnSharedContext() {
  if (cleanup_cb_)
    std::move(cleanup_cb_).Run(this);

  // If the shared context is lost, |shared_context_state_| will be null.
  if (!shared_context_state_) {
    texture_->RemoveLightweightRef(false);
  } else {
    base::Optional<ui::ScopedMakeCurrent> scoped_make_current;
    if (!shared_context_state_->IsCurrent(nullptr)) {
      scoped_make_current.emplace(shared_context_state_->context(),
                                  shared_context_state_->surface());
    }
    texture_->RemoveLightweightRef(true);
    shared_context_state_->RemoveContextLostObserver(this);
  }
  texture_ = nullptr;
}

TextureBase* AbstractTextureImplOnSharedContext::GetTextureBase() const {
  return texture_;
}

void AbstractTextureImplOnSharedContext::SetParameteri(GLenum pname,
                                                       GLint param) {
  NOTIMPLEMENTED();
}

void AbstractTextureImplOnSharedContext::BindStreamTextureImage(
    GLStreamTextureImage* image,
    GLuint service_id) {
  NOTIMPLEMENTED();
}

void AbstractTextureImplOnSharedContext::BindImage(gl::GLImage* image,
                                                   bool client_managed) {
  NOTIMPLEMENTED();
}

gl::GLImage* AbstractTextureImplOnSharedContext::GetImage() const {
  NOTIMPLEMENTED();
  return nullptr;
}

void AbstractTextureImplOnSharedContext::SetCleared() {
  NOTIMPLEMENTED();
}

void AbstractTextureImplOnSharedContext::SetCleanupCallback(
    CleanupCallback cb) {
  cleanup_cb_ = std::move(cb);
}

void AbstractTextureImplOnSharedContext::OnContextLost() {
  if (cleanup_cb_)
    std::move(cleanup_cb_).Run(this);
  shared_context_state_->RemoveContextLostObserver(this);
  shared_context_state_.reset();
}

}  // namespace gles2
}  // namespace gpu
