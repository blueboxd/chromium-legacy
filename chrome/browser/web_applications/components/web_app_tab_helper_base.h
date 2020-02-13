// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_WEB_APP_TAB_HELPER_BASE_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_WEB_APP_TAB_HELPER_BASE_H_

#include "chrome/browser/web_applications/components/web_app_id.h"
#include "content/public/browser/web_contents_user_data.h"

namespace base {
class UnguessableToken;
}

namespace web_app {

// Public interface for WebAppTabHelper.
class WebAppTabHelperBase
    : public content::WebContentsUserData<WebAppTabHelperBase> {
 public:
  using content::WebContentsUserData<WebAppTabHelperBase>::FromWebContents;

  virtual const AppId& GetAppId() const = 0;
  virtual void SetAppId(const AppId& app_id) = 0;

  virtual const base::UnguessableToken& GetAudioFocusGroupIdForTesting()
      const = 0;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_WEB_APP_TAB_HELPER_BASE_H_
