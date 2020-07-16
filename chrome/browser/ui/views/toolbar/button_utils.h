// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TOOLBAR_BUTTON_UTILS_H_
#define CHROME_BROWSER_UI_VIEWS_TOOLBAR_BUTTON_UTILS_H_

#include "build/build_config.h"

#if defined(OS_WIN)
// For Windows 10 and later, we use custom icons for minimal-ui web app
// Back and Reload buttons, to conform to the native OS' appearance.
// https://w3c.github.io/manifest/#dom-displaymodetype-minimal-ui
// TODO(http://crbug.com/1099607) Remove this once WebAppFrameToolbarView
// doesn't need this.
bool UseWindowsIconsForMinimalUI();
#endif

#endif  // CHROME_BROWSER_UI_VIEWS_TOOLBAR_BUTTON_UTILS_H_
