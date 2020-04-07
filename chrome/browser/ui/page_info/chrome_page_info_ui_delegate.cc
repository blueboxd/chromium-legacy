// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/page_info/chrome_page_info_ui_delegate.h"

#include "build/build_config.h"
#include "chrome/browser/permissions/permission_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/permissions/permission_manager.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_contents.h"
#include "url/gurl.h"

ChromePageInfoUiDelegate::ChromePageInfoUiDelegate(Profile* profile)
    : profile_(profile) {}

permissions::PermissionResult ChromePageInfoUiDelegate::GetPermissionStatus(
    ContentSettingsType type,
    const GURL& url) {
  return PermissionManagerFactory::GetForProfile(profile_)->GetPermissionStatus(
      type, url, url);
}

#if !defined(OS_ANDROID)

bool ChromePageInfoUiDelegate::IsBlockAutoPlayEnabled() {
  return profile_->GetPrefs()->GetBoolean(prefs::kBlockAutoplayEnabled);
}
#endif
