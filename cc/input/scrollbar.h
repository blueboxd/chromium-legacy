// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_INPUT_SCROLLBAR_H_
#define CC_INPUT_SCROLLBAR_H_

#include "cc/cc_export.h"
#include "cc/paint/paint_canvas.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"

static constexpr int kPixelsPerLineStep = 40;
static constexpr float kMinFractionToStepWhenPaging = 0.875f;

// Autoscrolling (on the main thread) happens by applying a delta every 50ms.
// Hence, pixels per second for a autoscroll cc animation can be calculated as:
// autoscroll velocity = delta / 0.05 sec = delta x 20
static constexpr float kAutoscrollMultiplier = 20.f;
static constexpr base::TimeDelta kInitialAutoscrollTimerDelay =
    base::TimeDelta::FromMilliseconds(250);

namespace cc {

enum ScrollbarOrientation { HORIZONTAL, VERTICAL };
enum ScrollDirection { SCROLL_BACKWARD, SCROLL_FORWARD };
enum AutoScrollState { NO_AUTOSCROLL, AUTOSCROLL_FORWARD, AUTOSCROLL_BACKWARD };

enum ScrollbarPart {
  THUMB,
  TRACK,
  TICKMARKS,
  BACK_BUTTON,
  FORWARD_BUTTON,
  BACK_TRACK,
  FORWARD_TRACK,
  NO_PART,
};

class Scrollbar {
 public:
  virtual ~Scrollbar() {}

  virtual ScrollbarOrientation Orientation() const = 0;
  virtual bool IsLeftSideVerticalScrollbar() const = 0;
  virtual gfx::Point Location() const = 0;
  virtual bool IsOverlay() const = 0;
  virtual bool HasThumb() const = 0;
  virtual int ThumbThickness() const = 0;
  virtual int ThumbLength() const = 0;
  virtual gfx::Rect TrackRect() const = 0;
  virtual gfx::Rect BackButtonRect() const = 0;
  virtual gfx::Rect ForwardButtonRect() const = 0;
  virtual float ThumbOpacity() const = 0;
  virtual bool HasTickmarks() const = 0;
  virtual bool NeedsPaintPart(ScrollbarPart part) const = 0;
  virtual void PaintPart(PaintCanvas* canvas,
                         ScrollbarPart part,
                         const gfx::Rect& content_rect) = 0;
  virtual bool UsesNinePatchThumbResource() const = 0;
  virtual gfx::Size NinePatchThumbCanvasSize() const = 0;
  virtual gfx::Rect NinePatchThumbAperture() const = 0;
};

}  // namespace cc

#endif  // CC_INPUT_SCROLLBAR_H_
