// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_EXTENSIONS_BOOKMARK_APP_TAB_HELPER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_EXTENSIONS_BOOKMARK_APP_TAB_HELPER_H_

#include "base/macros.h"
#include "chrome/browser/web_applications/components/web_app_tab_helper_base.h"

namespace content {
class WebContents;
}

namespace extensions {

class Extension;

// Allows to associate a tab with bookmark app.
class BookmarkAppTabHelper : public web_app::WebAppTabHelperBase {
 public:
  explicit BookmarkAppTabHelper(content::WebContents* web_contents);
  ~BookmarkAppTabHelper() override;

  // Should only be called through WebAppProvider::CreateTabHelper which ensures
  // the right tab helper is created based on the DesktopPWAsWithoutExtensions
  // feature.
  static BookmarkAppTabHelper* CreateForWebContents(
      content::WebContents* web_contents);

  // WebAppTabHelperBase:
  web_app::WebAppTabHelperBase* CloneForWebContents(
      content::WebContents* web_contents) const override;
  bool IsInAppWindow() const override;
  bool IsUserInstalled() const override;
  bool IsFromInstallButton() const override;

 private:
  // Get a pointer from app_id_. Semantically, we use app_id_ as a weak
  // reference. It might become nullptr in unforeseen circumstances (Uninstall).
  // TODO(loyso): Provide guarantees for app_id_. crbug.com/915034
  const Extension* GetExtension() const;

  DISALLOW_COPY_AND_ASSIGN(BookmarkAppTabHelper);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_EXTENSIONS_BOOKMARK_APP_TAB_HELPER_H_
