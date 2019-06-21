// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_CLIPBOARD_CLIPBOARD_TYPES_H_
#define UI_BASE_CLIPBOARD_CLIPBOARD_TYPES_H_

namespace ui {

// |ClipboardType| designates which clipboard buffer the action should be
// applied to.
enum class ClipboardType {
  kCopyPaste,
  kSelection,  // Only supported on systems running X11.
  kDrag,       // Only supported on Mac OS X.
  kMaxValue = kDrag
};

}  // namespace ui

#endif  // UI_BASE_CLIPBOARD_CLIPBOARD_TYPES_H_
