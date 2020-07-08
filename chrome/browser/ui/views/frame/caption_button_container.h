// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_CAPTION_BUTTON_CONTAINER_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_CAPTION_BUTTON_CONTAINER_H_

#include "ui/gfx/geometry/point.h"
#include "ui/views/view.h"

// Abstract base class for caption button containers. This allows ownership of
// caption buttons for certain frame types and situations (specifically handling
// the case of touch-tablet mode on Windows).
class CaptionButtonContainer : public views::View {
 public:
  void set_paint_frame_background(bool paint_frame_background) {
    paint_frame_background_ = paint_frame_background;
  }
  bool paint_frame_background() const { return paint_frame_background_; }

  // Tests to see if the specified |point| (which is expressed in this view's
  // coordinates and which must be within this view's bounds) is within one of
  // the caption buttons. Returns one of HitTestCompat enum defined in
  // ui/base/hit_test.h, HTCAPTION if the area hit would be part of the window's
  // drag handle, and HTNOWHERE otherwise.
  // See also ClientView::NonClientHitTest.
  virtual int NonClientHitTest(const gfx::Point& point) const = 0;

 protected:
  // views::View:
  void OnPaintBackground(gfx::Canvas* canvas) override;

 private:
  // Determines whether or not this container should paint its own background in
  // the appropriate browser frame color (true) or should paint on its existing
  // parent view's background (false). The default is false.
  //
  // This method is provided because the background color of the caption buttons
  // should match the background color of the tabstrip in normal browser mode,
  // or the frame in PWA and WebUI tablet tabstrip mode. See crbug.com/1099195
  // for an example of what happens if background painting is not disabled in
  // normal browser mode.
  bool paint_frame_background_ = false;
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_CAPTION_BUTTON_CONTAINER_H_
