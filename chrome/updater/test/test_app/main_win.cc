// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>

#include "chrome/updater/test/test_app/test_app.h"

int APIENTRY wWinMain(HINSTANCE instance, HINSTANCE prev, wchar_t*, int) {
  return updater::TestAppMain(0, nullptr);
}
