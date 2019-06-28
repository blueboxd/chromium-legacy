// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_DIRECT_MANIPULATION_HELPER_WIN_H_
#define CONTENT_BROWSER_RENDERER_HOST_DIRECT_MANIPULATION_HELPER_WIN_H_

#include <windows.h>

#include <directmanipulation.h>
#include <wrl.h>
#include <memory>
#include <string>

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "content/browser/renderer_host/direct_manipulation_event_handler_win.h"
#include "content/common/content_export.h"
#include "ui/gfx/geometry/size.h"

namespace ui {

class WindowEventTarget;

}  // namespace ui

namespace content {

class DirectManipulationBrowserTest;
class DirectManipulationUnitTest;

// TODO(crbug.com/914914) This is added for help us getting debug log on
// machine with scrolling issue on Windows Precision Touchpad. We will remove it
// after Windows Precision Touchpad scrolling issue fixed.
void DebugLogging(const std::string& s, HRESULT hr = 0);
bool LoggingEnabled();

// Windows 10 provides a new API called Direct Manipulation which generates
// smooth scroll and scale factor via IDirectManipulationViewportEventHandler
// on precision touchpad.
// 1. The foreground window is checked to see if it is a Direct Manipulation
//    consumer.
// 2. Call SetContact in Direct Manipulation takes over the following scrolling
//    when DM_POINTERHITTEST.
// 3. OnViewportStatusChanged will be called when the gesture phase change.
//    OnContentUpdated will be called when the gesture update.
class CONTENT_EXPORT DirectManipulationHelper {
 public:
  // Creates and initializes an instance of this class if Direct Manipulation is
  // enabled on the platform. Returns nullptr if it disabled or failed on
  // initialization.
  static std::unique_ptr<DirectManipulationHelper> CreateInstance(
      HWND window,
      ui::WindowEventTarget* event_target);

  // Creates and initializes an instance for testing.
  static std::unique_ptr<DirectManipulationHelper> CreateInstanceForTesting(
      ui::WindowEventTarget* event_target,
      Microsoft::WRL::ComPtr<IDirectManipulationViewport> viewport);

  ~DirectManipulationHelper();

  // Actives Direct Manipulation, call when window show.
  void Activate();

  // Deactivates Direct Manipulation, call when window show.
  void Deactivate();

  // Updates viewport size. Call it when window bounds updated.
  void SetSizeInPixels(const gfx::Size& size_in_pixels);

  // Reset for gesture end.
  HRESULT Reset(bool need_animtation);

  // Pass the pointer hit test to Direct Manipulation. Return true indicated we
  // need poll for new events every frame from here.
  bool OnPointerHitTest(WPARAM w_param, ui::WindowEventTarget* event_target);

  // On each frame poll new Direct Manipulation events. Return true if we still
  // need poll for new events on next frame, otherwise stop request need begin
  // frame.
  bool PollForNextEvent();

 private:
  friend class content::DirectManipulationBrowserTest;
  friend class DirectManipulationUnitTest;

  DirectManipulationHelper();

  // This function instantiates Direct Manipulation and creates a viewport for
  // the passed in |window|. Return false if initialize failed.
  bool Initialize(ui::WindowEventTarget* event_target);

  void SetDeviceScaleFactorForTesting(float factor);

  Microsoft::WRL::ComPtr<IDirectManipulationManager> manager_;
  Microsoft::WRL::ComPtr<IDirectManipulationUpdateManager> update_manager_;
  Microsoft::WRL::ComPtr<IDirectManipulationViewport> viewport_;
  Microsoft::WRL::ComPtr<DirectManipulationEventHandler> event_handler_;
  HWND window_;
  DWORD view_port_handler_cookie_;
  bool need_poll_events_ = false;
  gfx::Size viewport_size_in_pixels_;

  DISALLOW_COPY_AND_ASSIGN(DirectManipulationHelper);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_DIRECT_MANIPULATION_HELPER_WIN_H_
