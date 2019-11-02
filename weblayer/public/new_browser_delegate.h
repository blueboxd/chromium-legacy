// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_PUBLIC_NEW_BROWSER_DELEGATE_H_
#define WEBLAYER_PUBLIC_NEW_BROWSER_DELEGATE_H_

#include <memory>

namespace weblayer {

class BrowserController;

// Corresponds to type of browser the page requested.
enum class NewBrowserDisposition {
  // The new browser should be opened in the foreground.
  kForeground,

  // The new browser should be opened in the foreground.
  kBackground,

  // The page requested the browser be shown in a new window with minimal
  // browser UI. For example, no tabstrip.
  kNewPopup,
  // The page requested the browser be shown in a new window.
  kNewWindow,
};

// An interface that allows clients to handle requests for new browsers, or
// in web terms, a new popup/window (and random other things).
class NewBrowserDelegate {
 public:
  virtual void OnNewBrowser(std::unique_ptr<BrowserController> new_contents,
                            NewBrowserDisposition disposition) = 0;

 protected:
  virtual ~NewBrowserDelegate() {}
};

}  // namespace weblayer

#endif  // WEBLAYER_PUBLIC_NEW_BROWSER_DELEGATE_H_
