// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/page_info/chrome_page_info_delegate.h"

#include "build/build_config.h"
#include "chrome/browser/content_settings/tab_specific_content_settings.h"
#include "chrome/browser/permissions/permission_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/chrome_password_protection_service.h"
#include "components/permissions/permission_manager.h"
#include "components/permissions/permission_result.h"
#include "content/public/browser/web_contents.h"

#if BUILDFLAG(FULL_SAFE_BROWSING)
#include "chrome/browser/safe_browsing/chrome_password_protection_service.h"
#endif

ChromePageInfoDelegate::ChromePageInfoDelegate(
    content::WebContents* web_contents)
    : web_contents_(web_contents) {}

Profile* ChromePageInfoDelegate::GetProfile() const {
  return Profile::FromBrowserContext(web_contents_->GetBrowserContext());
}

bool ChromePageInfoDelegate::HasContentSettingChangedViaPageInfo(
    ContentSettingsType type) {
  return tab_specific_content_settings()->HasContentSettingChangedViaPageInfo(
      type);
}

const LocalSharedObjectsContainer& ChromePageInfoDelegate::GetAllowedObjects(
    const GURL& site_url) {
  return tab_specific_content_settings()->allowed_local_shared_objects();
}

const LocalSharedObjectsContainer& ChromePageInfoDelegate::GetBlockedObjects(
    const GURL& site_url) {
  return tab_specific_content_settings()->blocked_local_shared_objects();
}

int ChromePageInfoDelegate::GetFirstPartyAllowedCookiesCount(
    const GURL& site_url) {
  return GetAllowedObjects(site_url).GetObjectCountForDomain(site_url);
}

int ChromePageInfoDelegate::GetFirstPartyBlockedCookiesCount(
    const GURL& site_url) {
  return GetBlockedObjects(site_url).GetObjectCountForDomain(site_url);
}

int ChromePageInfoDelegate::GetThirdPartyAllowedCookiesCount(
    const GURL& site_url) {
  return GetAllowedObjects(site_url).GetObjectCount() -
         GetFirstPartyAllowedCookiesCount(site_url);
}

int ChromePageInfoDelegate::GetThirdPartyBlockedCookiesCount(
    const GURL& site_url) {
  return GetBlockedObjects(site_url).GetObjectCount() -
         GetFirstPartyBlockedCookiesCount(site_url);
}

#if BUILDFLAG(FULL_SAFE_BROWSING)
safe_browsing::ChromePasswordProtectionService*
ChromePageInfoDelegate::GetChromePasswordProtectionService() const {
  return safe_browsing::ChromePasswordProtectionService::
      GetPasswordProtectionService(GetProfile());
}

safe_browsing::PasswordProtectionService*
ChromePageInfoDelegate::GetPasswordProtectionService() const {
  return GetChromePasswordProtectionService();
}

void ChromePageInfoDelegate::OnUserActionOnPasswordUi(
    content::WebContents* web_contents,
    safe_browsing::WarningAction action) {
  auto* chrome_password_protection_service =
      GetChromePasswordProtectionService();
  DCHECK(chrome_password_protection_service);

  chrome_password_protection_service->OnUserAction(
      web_contents,
      chrome_password_protection_service
          ->reused_password_account_type_for_last_shown_warning(),
      safe_browsing::RequestOutcome::UNKNOWN,
      safe_browsing::LoginReputationClientResponse::VERDICT_TYPE_UNSPECIFIED,
      /*verdict_token=*/"", safe_browsing::WarningUIType::PAGE_INFO, action);
}

base::string16 ChromePageInfoDelegate::GetWarningDetailText() {
  std::vector<size_t> placeholder_offsets;
  auto* chrome_password_protection_service =
      GetChromePasswordProtectionService();

  // |password_protection_service| may be null in test.
  return chrome_password_protection_service
             ? chrome_password_protection_service->GetWarningDetailText(
                   chrome_password_protection_service
                       ->reused_password_account_type_for_last_shown_warning(),
                   &placeholder_offsets)
             : base::string16();
}
#endif

permissions::PermissionResult ChromePageInfoDelegate::GetPermissionStatus(
    ContentSettingsType type,
    const GURL& site_url) {
  // TODO(raymes): Use GetPermissionStatus() to retrieve information
  // about *all* permissions once it has default behaviour implemented for
  // ContentSettingTypes that aren't permissions.
  return PermissionManagerFactory::GetForProfile(GetProfile())
      ->GetPermissionStatus(type, site_url, site_url);
}
