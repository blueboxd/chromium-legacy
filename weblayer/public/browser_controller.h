// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_PUBLIC_BROWSER_CONTROLLER_H_
#define WEBLAYER_PUBLIC_BROWSER_CONTROLLER_H_

#include <algorithm>

#include "base/callback_forward.h"
#include "base/strings/string16.h"
#include "build/build_config.h"

namespace base {
class Value;
}

#if !defined(OS_ANDROID)
namespace views {
class WebView;
}
#endif

namespace weblayer {
class BrowserObserver;
class DownloadDelegate;
class FullscreenDelegate;
class NavigationController;
class NewBrowserDelegate;
class Profile;

// Represents a browser window that is navigable.
class BrowserController {
 public:
  static std::unique_ptr<BrowserController> Create(Profile* profile);

#if defined(OS_ANDROID)
  static BrowserController* GetLastControllerForTesting();
#endif

  virtual ~BrowserController() {}

  // Sets the DownloadDelegate. If none is set, downloads will be dropped.
  virtual void SetDownloadDelegate(DownloadDelegate* delegate) = 0;

  // Sets the FullscreenDelegate. Setting a non-null value implicitly enables
  // fullscreen.
  virtual void SetFullscreenDelegate(FullscreenDelegate* delegate) = 0;

  // Sets the NewBrowserDelegate. Setting a null value implicitly disables
  // popups.
  virtual void SetNewBrowserDelegate(NewBrowserDelegate* delegate) = 0;

  virtual void AddObserver(BrowserObserver* observer) = 0;

  virtual void RemoveObserver(BrowserObserver* observer) = 0;

  virtual NavigationController* GetNavigationController() = 0;

  using JavaScriptResultCallback = base::OnceCallback<void(base::Value)>;

  // Executes the script, and returns the result to the callback if provided. If
  // |use_separate_isolate| is true, runs the script in a separate v8 Isolate.
  // This uses more memory, but separates the injected scrips from scripts in
  // the page. This prevents any potentially malicious interaction between
  // first-party scripts in the page, and injected scripts. Use with caution,
  // only pass false for this argument if you know this isn't an issue or you
  // need to interact with first-party scripts.
  virtual void ExecuteScript(const base::string16& script,
                             bool use_separate_isolate,
                             JavaScriptResultCallback callback) = 0;

#if !defined(OS_ANDROID)
  // TODO: this isn't a stable API, so use it now for expediency in the C++ API,
  // but if we ever want to have backward or forward compatibility in C++ this
  // will have to be something else.
  virtual void AttachToView(views::WebView* web_view) = 0;
#endif
};

}  // namespace weblayer

#endif  // WEBLAYER_PUBLIC_BROWSER_CONTROLLER_H_
