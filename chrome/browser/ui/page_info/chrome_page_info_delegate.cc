// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/page_info/chrome_page_info_delegate.h"

#include "chrome/browser/content_settings/tab_specific_content_settings.h"
#include "content/public/browser/web_contents.h"

ChromePageInfoDelegate::ChromePageInfoDelegate(
    content::WebContents* web_contents)
    : web_contents_(web_contents) {}

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
