// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sessions/session_service_utils.h"

// TODO(crbug.com/990158): Add mappings for Browser::Type APP and DEVTOOLS.
sessions::SessionWindow::WindowType WindowTypeForBrowserType(
    Browser::Type type) {
  switch (type) {
    case Browser::TYPE_NORMAL:
      return sessions::SessionWindow::TYPE_NORMAL;
    case Browser::TYPE_POPUP:
    case Browser::TYPE_APP:
    case Browser::TYPE_DEVTOOLS:
      return sessions::SessionWindow::TYPE_POPUP;
  }
  NOTREACHED();
  return sessions::SessionWindow::TYPE_NORMAL;
}

Browser::Type BrowserTypeForWindowType(
    sessions::SessionWindow::WindowType type) {
  switch (type) {
    case sessions::SessionWindow::TYPE_NORMAL:
      return Browser::TYPE_NORMAL;
    case sessions::SessionWindow::TYPE_POPUP:
      return Browser::TYPE_POPUP;
  }
  NOTREACHED();
  return Browser::TYPE_NORMAL;
}
