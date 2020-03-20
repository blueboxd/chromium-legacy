// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PAGE_INFO_CHROME_PAGE_INFO_DELEGATE_H_
#define CHROME_BROWSER_UI_PAGE_INFO_CHROME_PAGE_INFO_DELEGATE_H_

#include "build/build_config.h"
#include "chrome/browser/content_settings/tab_specific_content_settings.h"
#include "chrome/browser/ui/page_info/page_info_delegate.h"
#include "content/public/browser/web_contents_user_data.h"

namespace content {
class WebContents;
}  // namespace content

class ChromePageInfoDelegate : public PageInfoDelegate {
 public:
  explicit ChromePageInfoDelegate(content::WebContents* web_contents);
  ~ChromePageInfoDelegate() override = default;

  // PageInfoDelegate implementation
  bool HasContentSettingChangedViaPageInfo(ContentSettingsType type) override;

 private:
  TabSpecificContentSettings* tab_specific_content_settings() const {
    TabSpecificContentSettings::CreateForWebContents(web_contents_);
    return TabSpecificContentSettings::FromWebContents(web_contents_);
  }
  content::WebContents* web_contents_;
};

#endif  // CHROME_BROWSER_UI_PAGE_INFO_CHROME_PAGE_INFO_DELEGATE_H_
