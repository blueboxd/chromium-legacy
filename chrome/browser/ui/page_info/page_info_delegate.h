// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PAGE_INFO_PAGE_INFO_DELEGATE_H_
#define CHROME_BROWSER_UI_PAGE_INFO_PAGE_INFO_DELEGATE_H_

#include "base/strings/string16.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/permissions/permission_result.h"
#include "components/safe_browsing/buildflags.h"
#include "components/safe_browsing/content/password_protection/metrics_util.h"

namespace safe_browsing {
class PasswordProtectionService;
}  // namespace safe_browsing

// PageInfoDelegate allows an embedder to customize PageInfo logic.
class PageInfoDelegate {
 public:
  virtual ~PageInfoDelegate() = default;

  // Whether the content setting of type |type| has changed via Page Info UI.
  virtual bool HasContentSettingChangedViaPageInfo(
      ContentSettingsType type) = 0;

  // Get counts of allowed and blocked cookies.
  virtual int GetFirstPartyAllowedCookiesCount(const GURL& site_url) = 0;
  virtual int GetFirstPartyBlockedCookiesCount(const GURL& site_url) = 0;
  virtual int GetThirdPartyAllowedCookiesCount(const GURL& site_url) = 0;
  virtual int GetThirdPartyBlockedCookiesCount(const GURL& site_url) = 0;

#if BUILDFLAG(FULL_SAFE_BROWSING)
  // Helper methods requiring access to PasswordProtectionService.
  virtual safe_browsing::PasswordProtectionService*
  GetPasswordProtectionService() const = 0;
  virtual void OnUserActionOnPasswordUi(
      content::WebContents* web_contents,
      safe_browsing::WarningAction action) = 0;
  virtual base::string16 GetWarningDetailText() = 0;
#endif
  // Get permission status for the permission associated with ContentSetting of
  // type |type|.
  virtual permissions::PermissionResult GetPermissionStatus(
      ContentSettingsType type,
      const GURL& site_url) = 0;
};

#endif  // CHROME_BROWSER_UI_PAGE_INFO_PAGE_INFO_DELEGATE_H_
