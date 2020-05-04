// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/test/mock_cast_content_window_delegate.h"

namespace chromecast {

MockCastContentWindowDelegate::MockCastContentWindowDelegate() {}

MockCastContentWindowDelegate::~MockCastContentWindowDelegate() = default;

void MockCastContentWindowDelegate::ConsumeGesture(
    GestureType gesture_type,
    GestureHandledCallback handled_callback) {
  ConsumeGesture(gesture_type,
                 base::AdaptCallbackForRepeating(std::move(handled_callback)));
}

std::string MockCastContentWindowDelegate::GetId() {
  return "mockContentWindowDelegate";
}

}  // namespace chromecast
