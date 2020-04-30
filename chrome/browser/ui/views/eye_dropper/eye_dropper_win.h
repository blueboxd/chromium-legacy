// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_EYE_DROPPER_EYE_DROPPER_WIN_H_
#define CHROME_BROWSER_UI_VIEWS_EYE_DROPPER_EYE_DROPPER_WIN_H_

#include <memory>

#include "base/optional.h"
#include "base/timer/timer.h"
#include "content/public/browser/eye_dropper.h"
#include "content/public/browser/eye_dropper_listener.h"
#include "content/public/browser/render_frame_host.h"
#include "ui/gfx/geometry/point.h"
#include "ui/views/widget/widget_delegate.h"

class EyeDropperWin : public content::EyeDropper,
                      public views::WidgetDelegateView {
 public:
  EyeDropperWin(content::RenderFrameHost* frame,
                content::EyeDropperListener* listener);
  EyeDropperWin(const EyeDropperWin&) = delete;
  EyeDropperWin& operator=(const EyeDropperWin&) = delete;
  ~EyeDropperWin() override;

 protected:
  // views::WidgetDelegateView:
  void OnPaint(gfx::Canvas* canvas) override;
  void WindowClosing() override;
  void DeleteDelegate() override;
  ui::ModalType GetModalType() const override;
  void OnWidgetMove() override;

 private:
  class PreEventDispatchHandler;
  class ViewPositionHandler;
  class ScreenCapturer;

  // Moves the view to the cursor position.
  void UpdatePosition();

  // Handles color selection and notifies the listener.
  void OnColorSelected();

  content::RenderFrameHost* render_frame_host_;

  // Receives the color selection.
  content::EyeDropperListener* listener_;

  std::unique_ptr<PreEventDispatchHandler> pre_dispatch_handler_;
  std::unique_ptr<ViewPositionHandler> view_position_handler_;
  std::unique_ptr<ScreenCapturer> screen_capturer_;
  base::Optional<SkColor> selected_color_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_EYE_DROPPER_EYE_DROPPER_WIN_H_
