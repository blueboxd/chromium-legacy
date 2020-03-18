// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_INPUT_SCROLL_INPUT_TYPE_H_
#define CC_INPUT_SCROLL_INPUT_TYPE_H_

namespace cc {

enum class ScrollInputType {
  kTouchscreen = 0,
  kWheel,
  kAutoscroll,
  kScrollbar,
  kMaxValue = kScrollbar,
};

}  // namespace cc

#endif  // CC_INPUT_SCROLL_INPUT_TYPE_H_
