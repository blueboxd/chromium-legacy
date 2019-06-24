// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/gl_image_dxgi_swap_chain.h"

#include "ui/gl/egl_util.h"
#include "ui/gl/gl_bindings.h"

#ifndef EGL_ANGLE_d3d_texture_client_buffer
#define EGL_ANGLE_d3d_texture_client_buffer 1
#define EGL_D3D_TEXTURE_ANGLE 0x33A3
#endif /* EGL_ANGLE_d3d_texture_client_buffer */

namespace gl {

namespace {

bool SwapChainSupportedBindFormat(gfx::BufferFormat format) {
  switch (format) {
    case gfx::BufferFormat::RGBA_8888:
    case gfx::BufferFormat::RGBX_8888:
    case gfx::BufferFormat::RGBA_F16:
      return true;
    default:
      return false;
  };
}

bool SwapChainHasAlpha(gfx::BufferFormat format) {
  DCHECK(SwapChainSupportedBindFormat(format));
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

EGLConfig ChooseCompatibleConfig(gfx::BufferFormat format, EGLDisplay display) {
  DCHECK(SwapChainSupportedBindFormat(format));

  const EGLint color_bits = format == gfx::BufferFormat::RGBA_F16 ? 16 : 8;
  const EGLint buffer_bind_to_texture = SwapChainHasAlpha(format)
                                            ? EGL_BIND_TO_TEXTURE_RGBA
                                            : EGL_BIND_TO_TEXTURE_RGB;
  const EGLint buffer_size =
      color_bits * 3 + (SwapChainHasAlpha(format) ? color_bits : 0);

  std::vector<EGLint> attrib_list = {
      EGL_RED_SIZE,           color_bits, EGL_GREEN_SIZE,   color_bits,
      EGL_BLUE_SIZE,          color_bits, EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,
      buffer_bind_to_texture, EGL_TRUE,   EGL_BUFFER_SIZE,  buffer_size};
  // It is assumed that EGL_EXT_pixel_format_float extension is supported by
  // ANGLE.
  if (format == gfx::BufferFormat::RGBA_F16) {
    attrib_list.push_back(EGL_COLOR_COMPONENT_TYPE_EXT);
    attrib_list.push_back(EGL_COLOR_COMPONENT_TYPE_FLOAT_EXT);
  }
  attrib_list.push_back(EGL_NONE);

  EGLint num_config = 0;
  EGLBoolean result =
      eglChooseConfig(display, attrib_list.data(), nullptr, 0, &num_config);
  if (result != EGL_TRUE)
    return nullptr;
  std::vector<EGLConfig> all_configs(num_config);
  result = eglChooseConfig(display, attrib_list.data(), all_configs.data(),
                           num_config, &num_config);
  if (result != EGL_TRUE)
    return nullptr;
  for (EGLConfig config : all_configs) {
    EGLint bits = 0;
    // Ensures that the config chosen has requested number of bits.
    if (!eglGetConfigAttrib(display, config, EGL_RED_SIZE, &bits) ||
        bits != color_bits) {
      continue;
    }

    if (!eglGetConfigAttrib(display, config, EGL_BLUE_SIZE, &bits) ||
        bits != color_bits) {
      continue;
    }

    if (!eglGetConfigAttrib(display, config, EGL_GREEN_SIZE, &bits) ||
        bits != color_bits) {
      continue;
    }

    if (SwapChainHasAlpha(format) &&
        (!eglGetConfigAttrib(display, config, EGL_ALPHA_SIZE, &bits) ||
         bits != color_bits)) {
      continue;
    }

    return config;
  }
  return nullptr;
}

EGLSurface CreatePbuffer(const Microsoft::WRL::ComPtr<ID3D11Texture2D>& texture,
                         gfx::BufferFormat format,
                         EGLConfig config,
                         EGLDisplay display,
                         unsigned target) {
  DCHECK(SwapChainSupportedBindFormat(format));

  D3D11_TEXTURE2D_DESC desc;
  texture->GetDesc(&desc);
  EGLint width = desc.Width;
  EGLint height = desc.Height;

  EGLint pBufferAttributes[] = {
      EGL_WIDTH,
      width,
      EGL_HEIGHT,
      height,
      EGL_TEXTURE_TARGET,
      EGL_TEXTURE_2D,
      EGL_TEXTURE_FORMAT,
      SwapChainHasAlpha(format) ? EGL_TEXTURE_RGBA : EGL_TEXTURE_RGB,
      EGL_NONE};

  return eglCreatePbufferFromClientBuffer(
      display, EGL_D3D_TEXTURE_ANGLE, texture.Get(), config, pBufferAttributes);
}

}  // anonymous namespace

GLImageDXGISwapChain::GLImageDXGISwapChain(
    const gfx::Size& size,
    gfx::BufferFormat buffer_format,
    Microsoft::WRL::ComPtr<ID3D11Texture2D> texture,
    Microsoft::WRL::ComPtr<IDXGISwapChain1> swap_chain)
    : size_(size),
      buffer_format_(buffer_format),
      texture_(texture),
      swap_chain_(swap_chain),
      display_(eglGetCurrentDisplay()),
      config_(ChooseCompatibleConfig(buffer_format, display_)) {
  DCHECK(texture_);
  DCHECK(swap_chain_);
  DCHECK(config_);
}

GLImage::BindOrCopy GLImageDXGISwapChain::ShouldBindOrCopy() {
  return BIND;
}

bool GLImageDXGISwapChain::BindTexImage(unsigned target) {
  DestroySurface();
  DCHECK(texture_);
  DCHECK_EQ(surface_, EGL_NO_SURFACE);
  DCHECK(config_);
  surface_ = CreatePbuffer(texture_, buffer_format_, config_, display_, target);
  if (surface_ == EGL_NO_SURFACE) {
    DLOG(ERROR) << "eglCreatePbufferSurface failed with error "
                << ui::GetLastEGLErrorString();
    return false;
  }

  return eglBindTexImage(display_, surface_, EGL_BACK_BUFFER) == EGL_TRUE;
}

void GLImageDXGISwapChain::ReleaseTexImage(unsigned target) {}

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

gfx::Size GLImageDXGISwapChain::GetSize() {
  return size_;
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

void GLImageDXGISwapChain::DestroySurface() {
  if (surface_ != EGL_NO_SURFACE) {
    if (!eglDestroySurface(display_, surface_)) {
      DLOG(ERROR) << "eglDestroySurface failed with error "
                  << ui::GetLastEGLErrorString();
    }
    surface_ = EGL_NO_SURFACE;
  }
}

GLImageDXGISwapChain::~GLImageDXGISwapChain() {
  DestroySurface();
}

}  // namespace gl
