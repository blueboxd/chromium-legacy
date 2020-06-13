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
  // Tests to see if the specified |point| (which is expressed in this view's
  // coordinates and which must be within this view's bounds) is within one of
  // the caption buttons. Returns one of HitTestCompat enum defined in
  // ui/base/hit_test.h, HTCAPTION if the area hit would be part of the window's
  // drag handle, and HTNOWHERE otherwise.
  // See also ClientView::NonClientHitTest.
  virtual int NonClientHitTest(const gfx::Point& point) const = 0;
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_CAPTION_BUTTON_CONTAINER_H_
