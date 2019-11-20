// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEBUI_WEB_UI_BROWSERTEST_UTIL_H_
#define CONTENT_BROWSER_WEBUI_WEB_UI_BROWSERTEST_UTIL_H_

#include <memory>

#include "content/browser/webui/content_web_ui_controller_factory.h"

namespace content {

// Returns WebUIControllers whose CSPs and headers can be controlled through
// query parameters.
// - "bindings" controls the bindings e.g. Mojo, chrome.send() or both, with
//   which the WebUIController will be created.
// - "noxfo" controls whether the "X-Frame-Options: DENY" header, which is
//   added by default, will be removed. Set to true to remove the header.
// - "childsrc" controls the child-src CSP. It's value is
//   "child-src 'self' chrome://web-ui-subframe/;" by default.
class TestWebUIControllerFactory : public WebUIControllerFactory {
 public:
  TestWebUIControllerFactory();
  TestWebUIControllerFactory(const TestWebUIControllerFactory&) = delete;
  void operator=(const TestWebUIControllerFactory&) = delete;

  // Set to true to remove the "X-Frame-Options: DENY" header from all new
  // WebUIControllers.
  void set_disable_xfo(bool disable) { disable_xfo_ = disable; }

  // WebUIControllerFactory
  std::unique_ptr<WebUIController> CreateWebUIControllerForURL(
      WebUI* web_ui,
      const GURL& url) override;
  WebUI::TypeID GetWebUIType(BrowserContext* browser_context,
                             const GURL& url) override;
  bool UseWebUIForURL(BrowserContext* browser_context,
                      const GURL& url) override;
  bool UseWebUIBindingsForURL(BrowserContext* browser_context,
                              const GURL& url) override;

 private:
  bool disable_xfo_ = false;
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEBUI_WEB_UI_BROWSERTEST_UTIL_H_
