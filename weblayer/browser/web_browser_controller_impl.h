// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_BROWSER_WEB_BROWSER_CONTROLLER_IMPL_H_
#define WEBLAYER_BROWSER_WEB_BROWSER_CONTROLLER_IMPL_H_

#include "weblayer/public/web_browser_controller.h"

namespace content {
class WebContents;
}

namespace weblayer {
class WebNavigationControllerImpl;

class WebBrowserControllerImpl : public WebBrowserController {
 public:
  WebBrowserControllerImpl(WebProfile* profile, const gfx::Size& initial_size);
  ~WebBrowserControllerImpl() override;

  // WebBrowserController implementation:
  WebNavigationController* GetWebNavigationController() override;
  void AttachToView(views::WebView* web_view) override;

  content::WebContents* web_contents() const { return web_contents_.get(); }

 private:
  WebProfile* profile_;
  std::unique_ptr<content::WebContents> web_contents_;
  std::unique_ptr<WebNavigationControllerImpl> web_navigation_controller_;
};

}  // namespace weblayer

#endif  // WEBLAYER_BROWSER_WEB_BROWSER_CONTROLLER_IMPL_H_
