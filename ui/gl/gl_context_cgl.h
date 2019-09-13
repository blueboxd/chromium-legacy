// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_GL_CONTEXT_CGL_H_
#define UI_GL_GL_CONTEXT_CGL_H_

#include <OpenGL/CGLTypes.h>

#include <map>
#include <memory>

#include "base/macros.h"
#include "ui/gfx/color_space.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_export.h"

namespace gl {

class GLFence;
class GLSurface;

// Encapsulates a CGL OpenGL context.
class GL_EXPORT GLContextCGL : public GLContextReal {
 public:
  explicit GLContextCGL(GLShareGroup* share_group);

  // Implement GLContext.
  bool Initialize(GLSurface* compatible_surface,
                  const GLContextAttribs& attribs) override;
  bool MakeCurrent(GLSurface* surface) override;
  void ReleaseCurrent(GLSurface* surface) override;
  bool IsCurrent(GLSurface* surface) override;
  void* GetHandle() override;
  void SetSafeToForceGpuSwitch() override;
  bool ForceGpuSwitchIfNeeded() override;
  YUVToRGBConverter* GetYUVToRGBConverter(
      const gfx::ColorSpace& color_space) override;
  uint64_t BackpressureFenceCreate() override;
  void BackpressureFenceWait(uint64_t fence) override;
  void FlushForDriverCrashWorkaround() override;
  void SetVisibility(bool visibility) override;

 protected:
  ~GLContextCGL() override;

 private:
  void Destroy();
  GpuPreference GetGpuPreference();

  void* context_ = nullptr;
  GpuPreference gpu_preference_ = GpuPreference::kLowPower;
  std::map<gfx::ColorSpace, std::unique_ptr<YUVToRGBConverter>>
      yuv_to_rgb_converters_;

  std::map<uint64_t, std::unique_ptr<GLFence>> backpressure_fences_;
  uint64_t next_backpressure_fence_ = 0;

  int screen_ = -1;
  int renderer_id_ = -1;
  bool safe_to_force_gpu_switch_ = true;
  bool is_high_performance_context_ = false;

  // Debugging for https://crbug.com/863817
  bool has_switched_gpus_ = false;

  DISALLOW_COPY_AND_ASSIGN(GLContextCGL);
};

}  // namespace gl

#endif  // UI_GL_GL_CONTEXT_CGL_H_
