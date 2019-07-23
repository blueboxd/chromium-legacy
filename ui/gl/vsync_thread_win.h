// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_VSYNC_THREAD_WIN_H_
#define UI_GL_VSYNC_THREAD_WIN_H_

#include <d3d11.h>
#include <windows.h>
#include <wrl/client.h>

#include "base/threading/thread.h"
#include "ui/gl/gl_export.h"

namespace gl {

// Helper class that manages a thread for calling IDXGIOutput::WaitForVBlank()
// for the output corresponding to the given |window|, and runs |callback| on
// the same thread.  The callback can be enabled or disabled via SetEnabled().
// This is used by DirectCompositionSurfaceWin to plumb vsync signal back to the
// display compositor's BeginFrameSource.
class GL_EXPORT VSyncThreadWin {
 public:
  using VSyncCallback =
      base::RepeatingCallback<void(base::TimeTicks, base::TimeDelta)>;
  VSyncThreadWin(HWND window,
                 Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device,
                 VSyncCallback callback);
  ~VSyncThreadWin();

  void SetEnabled(bool enabled);

 private:
  void WaitForVSync();

  base::Thread vsync_thread_;

  // Used on vsync thread only after initialization.
  const HWND window_;
  const Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device_;
  const VSyncCallback callback_;

  // Used on vsync thread exclusively.
  HMONITOR window_monitor_ = nullptr;
  Microsoft::WRL::ComPtr<IDXGIOutput> window_output_;

  base::Lock lock_;
  bool GUARDED_BY(lock_) enabled_ = false;
  bool GUARDED_BY(lock_) started_ = false;

  DISALLOW_COPY_AND_ASSIGN(VSyncThreadWin);
};

}  // namespace gl

#endif  // UI_GL_VSYNC_THREAD_WIN_H_
