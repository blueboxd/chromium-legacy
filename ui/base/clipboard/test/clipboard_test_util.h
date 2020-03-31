// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_CLIPBOARD_TEST_CLIPBOARD_TEST_UTIL_H_
#define UI_BASE_CLIPBOARD_TEST_CLIPBOARD_TEST_UTIL_H_

#include "base/macros.h"

class SkBitmap;

namespace ui {

class Clipboard;

namespace clipboard_test_util {

// Helper function to read image from clipboard synchronously.
SkBitmap ReadImage(Clipboard* clipboard);

}  // namespace clipboard_test_util

}  // namespace ui

#endif  // UI_BASE_CLIPBOARD_TEST_CLIPBOARD_TEST_UTIL_H_
