// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_PUBLIC_WEB_BROWSER_CONTROLLER_H_
#define WEBLAYER_PUBLIC_WEB_BROWSER_CONTROLLER_H_

#include <algorithm>

namespace gfx {
class Size;
}

namespace views {
class WebView;
}

namespace weblayer {
class WebProfile;
class WebNavigationController;

// Represents a browser window that is navigable.
class WebBrowserController {
 public:
  // Pass an empty |path| for an in-memory profile.
  static std::unique_ptr<WebBrowserController> Create(
      WebProfile* profile,
      const gfx::Size& initial_size);

  virtual ~WebBrowserController() {}

  virtual WebNavigationController* GetWebNavigationController() = 0;

  // TODO: this isn't a stable API, so use it now for expediency in the C++ API,
  // but if we ever want to have backward or forward compatibility in C++ this
  // will have to be something else.
  virtual void AttachToView(views::WebView* web_view) = 0;
};

}  // namespace weblayer

#endif  // WEBLAYER_PUBLIC_WEB_BROWSER_CONTROLLER_H_
