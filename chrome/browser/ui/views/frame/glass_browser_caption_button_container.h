// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_GLASS_BROWSER_CAPTION_BUTTON_CONTAINER_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_GLASS_BROWSER_CAPTION_BUTTON_CONTAINER_H_

#include "base/scoped_observer.h"
#include "chrome/browser/ui/views/frame/caption_button_container.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"

class GlassBrowserFrameView;
class Windows10CaptionButton;

// Provides a container for Windows 10 caption buttons that can be moved between
// frame and browser window as needed. When extended horizontally, becomes a
// grab bar for moving the window.
class GlassBrowserCaptionButtonContainer : public CaptionButtonContainer,
                                           public views::WidgetObserver {
 public:
  explicit GlassBrowserCaptionButtonContainer(
      GlassBrowserFrameView* frame_view);
  ~GlassBrowserCaptionButtonContainer() override;

  // CaptionButtonContainer:
  int NonClientHitTest(const gfx::Point& point) const override;

 private:
  friend class GlassBrowserFrameView;

  // views::View:
  void AddedToWidget() override;

  // views::WidgetObserver:
  void OnWidgetBoundsChanged(views::Widget* widget,
                             const gfx::Rect& new_bounds) override;

  void ResetWindowControls();
  void ButtonPressed(views::Button* sender);

  // Sets caption button visibility based on window state. Only one of maximize
  // or restore button should ever be visible at the same time.
  void UpdateButtonVisibility();

  GlassBrowserFrameView* const frame_view_;
  Windows10CaptionButton* const minimize_button_;
  Windows10CaptionButton* const maximize_button_;
  Windows10CaptionButton* const restore_button_;
  Windows10CaptionButton* const close_button_;

  ScopedObserver<views::Widget, views::WidgetObserver> widget_observer_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_GLASS_BROWSER_CAPTION_BUTTON_CONTAINER_H_
