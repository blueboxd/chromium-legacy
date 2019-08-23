// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_BROWSER_WEB_NAVIGATION_CONTROLLER_IMPL_H_
#define WEBLAYER_BROWSER_WEB_NAVIGATION_CONTROLLER_IMPL_H_

#include "weblayer/public/web_navigation_controller.h"

namespace weblayer {
class WebBrowserControllerImpl;

class WebNavigationControllerImpl : public WebNavigationController {
 public:
  explicit WebNavigationControllerImpl(
      WebBrowserControllerImpl* web_browser_controller);
  ~WebNavigationControllerImpl() override;

  // WebNavigationController implementation:
  void Navigate(const GURL& url) override;

  void GoBack() override;

  void GoForward() override;

  void Reload() override;

  void Stop() override;

 private:
  WebBrowserControllerImpl* web_browser_controller_;
};

}  // namespace weblayer

#endif  // WEBLAYER_BROWSER_WEB_NAVIGATION_CONTROLLER_IMPL_H_
