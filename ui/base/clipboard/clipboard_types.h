// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_CLIPBOARD_CLIPBOARD_TYPES_H_
#define UI_BASE_CLIPBOARD_CLIPBOARD_TYPES_H_

namespace ui {

// |ClipboardType| designates which clipboard the action should be applied to.
enum ClipboardType {
  CLIPBOARD_TYPE_COPY_PASTE,
  CLIPBOARD_TYPE_SELECTION,  // Only supported on systems running X11.
  CLIPBOARD_TYPE_DRAG,       // Only supported on Mac OS X.
  CLIPBOARD_TYPE_LAST = CLIPBOARD_TYPE_DRAG
};

}  // namespace ui

#endif  // UI_BASE_CLIPBOARD_CLIPBOARD_TYPES_H_
