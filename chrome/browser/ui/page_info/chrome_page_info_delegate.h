// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PAGE_INFO_CHROME_PAGE_INFO_DELEGATE_H_
#define CHROME_BROWSER_UI_PAGE_INFO_CHROME_PAGE_INFO_DELEGATE_H_

#include "build/build_config.h"
#include "chrome/browser/content_settings/local_shared_objects_container.h"
#include "components/page_info/page_info_delegate.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_user_data.h"
#include "url/gurl.h"

namespace permissions {
class ChooserContextBase;
}

namespace safe_browsing {
class PasswordProtectionService;
class ChromePasswordProtectionService;
}  // namespace safe_browsing

class Profile;

namespace permissions {
class PermissionDecisionAutoBlocker;
}  // namespace permissions

class StatefulSSLHostStateDelegate;
class TabSpecificContentSettings;

class ChromePageInfoDelegate : public PageInfoDelegate {
 public:
  explicit ChromePageInfoDelegate(content::WebContents* web_contents);
  ~ChromePageInfoDelegate() override = default;

  // PageInfoDelegate implementation
  permissions::ChooserContextBase* GetChooserContext(
      ContentSettingsType type) override;
  bool HasContentSettingChangedViaPageInfo(ContentSettingsType type) override;
  void ContentSettingChangedViaPageInfo(ContentSettingsType type) override;
  int GetFirstPartyAllowedCookiesCount(const GURL& site_url) override;
  int GetFirstPartyBlockedCookiesCount(const GURL& site_url) override;
  int GetThirdPartyAllowedCookiesCount(const GURL& site_url) override;
  int GetThirdPartyBlockedCookiesCount(const GURL& site_url) override;
#if BUILDFLAG(FULL_SAFE_BROWSING)
  safe_browsing::PasswordProtectionService* GetPasswordProtectionService()
      const override;
  void OnUserActionOnPasswordUi(content::WebContents* web_contents,
                                safe_browsing::WarningAction action) override;
  base::string16 GetWarningDetailText() override;
#endif
  permissions::PermissionResult GetPermissionStatus(
      ContentSettingsType type,
      const GURL& site_url) override;

#if !defined(OS_ANDROID)
  bool CreateInfoBarDelegate() override;
  void ShowSiteSettings(const GURL& site_url) override;
#endif

  permissions::PermissionDecisionAutoBlocker* GetPermissionDecisionAutoblocker()
      override;
  StatefulSSLHostStateDelegate* GetStatefulSSLHostStateDelegate() override;
  HostContentSettingsMap* GetContentSettings() override;
  bool IsContentDisplayedInVrHeadset() override;

 private:
  TabSpecificContentSettings* GetTabSpecificContentSettings() const;
  const LocalSharedObjectsContainer& GetAllowedObjects(const GURL& site_url);
  const LocalSharedObjectsContainer& GetBlockedObjects(const GURL& site_url);
  Profile* GetProfile() const;
#if BUILDFLAG(FULL_SAFE_BROWSING)
  safe_browsing::ChromePasswordProtectionService*
  GetChromePasswordProtectionService() const;
#endif
  content::WebContents* web_contents_;
};

#endif  // CHROME_BROWSER_UI_PAGE_INFO_CHROME_PAGE_INFO_DELEGATE_H_
