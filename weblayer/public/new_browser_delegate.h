// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_PUBLIC_NEW_BROWSER_DELEGATE_H_
#define WEBLAYER_PUBLIC_NEW_BROWSER_DELEGATE_H_

#include <memory>

namespace weblayer {

class BrowserController;

// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.weblayer_private
// GENERATED_JAVA_CLASS_NAME_OVERRIDE: ImplNewBrowserType
// Corresponds to type of browser the page requested.
enum class NewBrowserType {
  // The new browser should be opened in the foreground.
  FOREGROUND_TAB = 0,

  // The new browser should be opened in the foreground.
  BACKGROUND_TAB,

  // The page requested the browser be shown in a new window with minimal
  // browser UI. For example, no tabstrip.
  NEW_POPUP,

  // The page requested the browser be shown in a new window.
  NEW_WINDOW,
};

// An interface that allows clients to handle requests for new browsers, or
// in web terms, a new popup/window (and random other things).
class NewBrowserDelegate {
 public:
  virtual void OnNewBrowser(std::unique_ptr<BrowserController> new_contents,
                            NewBrowserType type) = 0;

 protected:
  virtual ~NewBrowserDelegate() {}
};

}  // namespace weblayer

#endif  // WEBLAYER_PUBLIC_NEW_BROWSER_DELEGATE_H_
