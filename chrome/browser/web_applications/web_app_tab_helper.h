// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_TAB_HELPER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_TAB_HELPER_H_

#include "base/macros.h"
#include "chrome/browser/web_applications/components/web_app_tab_helper_base.h"

namespace content {
class WebContents;
}

namespace web_app {

// Allows to associate a tab with web app.
class WebAppTabHelper : public WebAppTabHelperBase {
 public:
  explicit WebAppTabHelper(content::WebContents* web_contents);
  ~WebAppTabHelper() override;

  // Should only be called through WebAppProvider::CreateTabHelper which ensures
  // the right tab helper is created based on the DesktopPWAsWithoutExtensions
  // feature.
  static WebAppTabHelper* CreateForWebContents(
      content::WebContents* web_contents);

  // WebAppTabHelperBase:
  WebAppTabHelperBase* CloneForWebContents(
      content::WebContents* web_contents) const override;
  bool IsInAppWindow() const override;
  bool IsUserInstalled() const override;
  bool IsFromInstallButton() const override;

 private:
  DISALLOW_COPY_AND_ASSIGN(WebAppTabHelper);
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_TAB_HELPER_H_
