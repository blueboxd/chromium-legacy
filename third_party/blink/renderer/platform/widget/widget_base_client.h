// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_WIDGET_WIDGET_BASE_CLIENT_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_WIDGET_WIDGET_BASE_CLIENT_H_

#include "base/time/time.h"

namespace blink {

// This class is part of the foundation of all widgets. It provides
// callbacks from the compositing infrastructure that the individual widgets
// will need to implement.
class WidgetBaseClient {
 public:
  // Dispatch any pending input. This method will called before
  // dispatching a RequestAnimationFrame to the widget.
  virtual void DispatchRafAlignedInput(base::TimeTicks frame_time) = 0;

  // Called to update the document lifecycle, advance the state of animations
  // and dispatch rAF.
  virtual void BeginMainFrame(base::TimeTicks frame_time) = 0;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_WIDGET_WIDGET_BASE_CLIENT_H_
