// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_DIRECT_MANIPULATION_EVENT_HANDLER_WIN_H_
#define CONTENT_BROWSER_RENDERER_HOST_DIRECT_MANIPULATION_EVENT_HANDLER_WIN_H_

#include <windows.h>

#include <directmanipulation.h>
#include <wrl.h>

#include "base/macros.h"

namespace ui {

class WindowEventTarget;

}  // namespace ui

namespace content {

class DirectManipulationHelper;
class DirectManipulationUnitTest;

// DirectManipulationEventHandler receives status update and gesture events from
// Direct Manipulation API.
class DirectManipulationEventHandler
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<
              Microsoft::WRL::RuntimeClassType::ClassicCom>,
          Microsoft::WRL::Implements<
              Microsoft::WRL::RuntimeClassFlags<
                  Microsoft::WRL::RuntimeClassType::ClassicCom>,
              Microsoft::WRL::FtmBase,
              IDirectManipulationViewportEventHandler>> {
 public:
  explicit DirectManipulationEventHandler(DirectManipulationHelper* helper);

  // WindowEventTarget updates for every DM_POINTERHITTEST in case window
  // hierarchy changed.
  void SetWindowEventTarget(ui::WindowEventTarget* event_target);

  void SetDeviceScaleFactor(float device_scale_factor);

 private:
  friend DirectManipulationUnitTest;

  // DirectManipulationEventHandler();
  ~DirectManipulationEventHandler() override;

  enum class GestureState { kNone, kScroll, kFling, kPinch };

  void TransitionToState(GestureState gesture);

  HRESULT STDMETHODCALLTYPE
  OnViewportStatusChanged(_In_ IDirectManipulationViewport* viewport,
                          _In_ DIRECTMANIPULATION_STATUS current,
                          _In_ DIRECTMANIPULATION_STATUS previous) override;

  HRESULT STDMETHODCALLTYPE
  OnViewportUpdated(_In_ IDirectManipulationViewport* viewport) override;

  HRESULT STDMETHODCALLTYPE
  OnContentUpdated(_In_ IDirectManipulationViewport* viewport,
                   _In_ IDirectManipulationContent* content) override;

  DirectManipulationHelper* helper_ = nullptr;
  ui::WindowEventTarget* event_target_ = nullptr;
  float device_scale_factor_ = 1.0f;
  float last_scale_ = 1.0f;
  int last_x_offset_ = 0;
  int last_y_offset_ = 0;
  bool first_ready_ = false;
  bool should_send_scroll_begin_ = false;

  // Current recognized gesture from Direct Manipulation.
  GestureState gesture_state_ = GestureState::kNone;

  DISALLOW_COPY_AND_ASSIGN(DirectManipulationEventHandler);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_DIRECT_MANIPULATION_EVENT_HANDLER_WIN_H_
