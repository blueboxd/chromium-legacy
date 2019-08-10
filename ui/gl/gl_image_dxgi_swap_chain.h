// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_GL_IMAGE_DXGI_SWAP_CHAIN_H_
#define UI_GL_GL_IMAGE_DXGI_SWAP_CHAIN_H_

#include <windows.h>
#include <d3d11.h>
#include <dxgi1_2.h>
#include <wrl/client.h>

#include "ui/gfx/buffer_types.h"
#include "ui/gl/gl_export.h"
#include "ui/gl/gl_image.h"

namespace gl {

class GL_EXPORT GLImageDXGISwapChain : public GLImage {
 public:
  GLImageDXGISwapChain(const gfx::Size& size,
                       gfx::BufferFormat buffer_format,
                       Microsoft::WRL::ComPtr<ID3D11Texture2D> texture,
                       Microsoft::WRL::ComPtr<IDXGISwapChain1> swap_chain);

  // Safe downcast. Returns nullptr on failure.
  static GLImageDXGISwapChain* FromGLImage(GLImage* image);

  bool Initialize();

  // GLImage implementation
  Type GetType() const override;
  BindOrCopy ShouldBindOrCopy() override;
  gfx::Size GetSize() override;
  unsigned GetInternalFormat() override;
  bool BindTexImage(unsigned target) override;
  void ReleaseTexImage(unsigned target) override {}
  bool CopyTexImage(unsigned target) override;
  bool CopyTexSubImage(unsigned target,
                       const gfx::Point& offset,
                       const gfx::Rect& rect) override;
  void Flush() override {}
  void OnMemoryDump(base::trace_event::ProcessMemoryDump* pmd,
                    uint64_t process_tracing_id,
                    const std::string& dump_name) override;
  bool ScheduleOverlayPlane(gfx::AcceleratedWidget widget,
                            int z_order,
                            gfx::OverlayTransform transform,
                            const gfx::Rect& bounds_rect,
                            const gfx::RectF& crop_rect,
                            bool enable_blend,
                            std::unique_ptr<gfx::GpuFence> gpu_fence) override;

  const Microsoft::WRL::ComPtr<ID3D11Texture2D>& texture() const {
    return texture_;
  }

  const Microsoft::WRL::ComPtr<IDXGISwapChain1>& swap_chain() const {
    return swap_chain_;
  }

 private:
  ~GLImageDXGISwapChain() override;

  const gfx::Size size_;
  const gfx::BufferFormat buffer_format_;
  void* egl_image_ = nullptr; /* EGLImageKHR */
  Microsoft::WRL::ComPtr<ID3D11Texture2D> texture_;
  Microsoft::WRL::ComPtr<IDXGISwapChain1> swap_chain_;

  DISALLOW_COPY_AND_ASSIGN(GLImageDXGISwapChain);
};

}  // namespace gl

#endif  // UI_GL_GL_IMAGE_DXGI_SWAP_CHAIN_H_
