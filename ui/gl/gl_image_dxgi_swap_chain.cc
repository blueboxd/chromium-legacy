// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/gl_image_dxgi_swap_chain.h"

#include "ui/gl/egl_util.h"
#include "ui/gl/gl_bindings.h"

#ifndef EGL_ANGLE_image_d3d11_texture
#define EGL_D3D11_TEXTURE_ANGLE 0x3484
#endif /* EGL_ANGLE_image_d3d11_texture */

namespace gl {

namespace {

bool SwapChainHasAlpha(gfx::BufferFormat format) {
  switch (format) {
    case gfx::BufferFormat::RGBA_8888:
    case gfx::BufferFormat::RGBA_F16:
      return true;
    case gfx::BufferFormat::RGBX_8888:
      return false;
    default:
      NOTREACHED();
      return false;
  };
}

}  // anonymous namespace

GLImageDXGISwapChain::GLImageDXGISwapChain(
    const gfx::Size& size,
    gfx::BufferFormat buffer_format,
    Microsoft::WRL::ComPtr<ID3D11Texture2D> texture,
    Microsoft::WRL::ComPtr<IDXGISwapChain1> swap_chain)
    : GLImageEGL(size),
      buffer_format_(buffer_format),
      texture_(texture),
      swap_chain_(swap_chain) {
  DCHECK(texture_);
  DCHECK(swap_chain_);
}

// static
GLImageDXGISwapChain* GLImageDXGISwapChain::FromGLImage(GLImage* image) {
  if (!image || image->GetType() != Type::DXGI_SWAP_CHAIN)
    return nullptr;
  return static_cast<GLImageDXGISwapChain*>(image);
}

bool GLImageDXGISwapChain::Initialize() {
  DCHECK(texture_);
  const EGLint attribs[] = {EGL_NONE};
  return GLImageEGL::Initialize(EGL_NO_CONTEXT, EGL_D3D11_TEXTURE_ANGLE,
                                static_cast<EGLClientBuffer>(texture_.Get()),
                                attribs);
}

bool GLImageDXGISwapChain::CopyTexImage(unsigned target) {
  return false;
}

bool GLImageDXGISwapChain::CopyTexSubImage(unsigned target,
                                           const gfx::Point& offset,
                                           const gfx::Rect& rect) {
  return false;
}

void GLImageDXGISwapChain::Flush() {}

unsigned GLImageDXGISwapChain::GetInternalFormat() {
  return SwapChainHasAlpha(buffer_format_) ? GL_RGBA : GL_RGB;
}

void GLImageDXGISwapChain::OnMemoryDump(
    base::trace_event::ProcessMemoryDump* pmd,
    uint64_t process_tracing_id,
    const std::string& dump_name) {}

GLImage::Type GLImageDXGISwapChain::GetType() const {
  return Type::DXGI_SWAP_CHAIN;
}

bool GLImageDXGISwapChain::ScheduleOverlayPlane(
    gfx::AcceleratedWidget widget,
    int z_order,
    gfx::OverlayTransform transform,
    const gfx::Rect& bounds_rect,
    const gfx::RectF& crop_rect,
    bool enable_blend,
    std::unique_ptr<gfx::GpuFence> gpu_fence) {
  NOTREACHED();
  return false;
}

GLImageDXGISwapChain::~GLImageDXGISwapChain() {}

}  // namespace gl
