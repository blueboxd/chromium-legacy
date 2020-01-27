// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_FULLSCREEN_FULLSCREEN_FEATURES_H_
#define IOS_CHROME_BROWSER_UI_FULLSCREEN_FULLSCREEN_FEATURES_H_

#include "base/feature_list.h"
#include "components/flags_ui/feature_entry.h"

namespace fullscreen {
namespace features {

// Feature used by finch config to enable smooth scrolling when the default
// viewport adjustment experiment is selected via command line switches.
extern const base::Feature kSmoothScrollingDefault;

// Feature used to enable scoping FullscreenController to a Browser.
extern const base::Feature kFullscreenControllerBrowserScoped;

// Convenience method for determining when to adjust the viewport by resizing
// WKWebView or using smooth scrolling.
bool ShouldUseSmoothScrolling();

// Convenience method for determining when FullscreenController is stored and
// retrieved using the Browser instead of the BrowserState.
bool ShouldScopeFullscreenControllerToBrowser();

}  // namespace features
}  // namespace fullscreen

#endif  // IOS_CHROME_BROWSER_UI_FULLSCREEN_FULLSCREEN_FEATURES_H_
