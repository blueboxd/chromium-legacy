// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_IME_VIRTUAL_KEYBOARD_VISIBILITY_REQUEST_H_
#define UI_BASE_IME_VIRTUAL_KEYBOARD_VISIBILITY_REQUEST_H_

namespace ui {

// This mode corresponds to VirtualKeyboard API show/hide.
// https://github.com/MicrosoftEdge/MSEdgeExplainers/blob/master/VirtualKeyboardPolicy/explainer.md
enum class VirtualKeyboardVisibilityRequest {
  SHOW,
  HIDE,
  NONE,
  MAX = NONE,
};

}  // namespace ui

#endif  // UI_BASE_IME_VIRTUAL_KEYBOARD_VISIBILITY_REQUEST_H_
