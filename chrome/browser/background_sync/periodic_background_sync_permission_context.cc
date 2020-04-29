// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/background_sync/periodic_background_sync_permission_context.h"

#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/installable/installable_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "content/public/browser/browser_thread.h"
#include "third_party/blink/public/mojom/feature_policy/feature_policy.mojom.h"

#if defined(OS_ANDROID)
#include "chrome/browser/android/shortcut_helper.h"
#endif

PeriodicBackgroundSyncPermissionContext::
    PeriodicBackgroundSyncPermissionContext(
        content::BrowserContext* browser_context)
    : PermissionContextBase(browser_context,
                            ContentSettingsType::PERIODIC_BACKGROUND_SYNC,
                            blink::mojom::FeaturePolicyFeature::kNotFound) {}

PeriodicBackgroundSyncPermissionContext::
    ~PeriodicBackgroundSyncPermissionContext() = default;

bool PeriodicBackgroundSyncPermissionContext::IsPwaInstalled(
    const GURL& origin) const {
  // Because we're only passed the requesting origin from the permissions
  // infrastructure, we can't match the scope of installed PWAs to the exact URL
  // of the permission request. We instead look for any installed PWA for the
  // requesting origin. With this logic, if there's already a PWA installed for
  // google.com/travel, and a request to register Periodic Background Sync comes
  // in from google.com/maps, this method will return true and registration will
  // succeed, provided other required conditions are met.
  return DoesOriginContainAnyInstalledWebApp(browser_context(), origin);
}

#if defined(OS_ANDROID)
bool PeriodicBackgroundSyncPermissionContext::IsTwaInstalled(
    const GURL& origin) const {
  return ShortcutHelper::DoesOriginContainAnyInstalledTrustedWebActivity(
      origin);
}
#endif

bool PeriodicBackgroundSyncPermissionContext::IsRestrictedToSecureOrigins()
    const {
  return true;
}

ContentSetting
PeriodicBackgroundSyncPermissionContext::GetPermissionStatusInternal(
    content::RenderFrameHost* render_frame_host,
    const GURL& requesting_origin,
    const GURL& embedding_origin) const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

#if defined(OS_ANDROID)
  if (IsTwaInstalled(requesting_origin))
    return CONTENT_SETTING_ALLOW;
#endif

  if (!IsPwaInstalled(requesting_origin))
    return CONTENT_SETTING_BLOCK;

  // PWA installed. Check for one-shot Background Sync content setting.
  // Expected values are CONTENT_SETTING_BLOCK or CONTENT_SETTING_ALLOW.
  auto* host_content_settings_map =
      HostContentSettingsMapFactory::GetForProfile(browser_context());
  DCHECK(host_content_settings_map);

  auto content_setting = host_content_settings_map->GetContentSetting(
      requesting_origin, embedding_origin, ContentSettingsType::BACKGROUND_SYNC,
      /* resource_identifier= */ std::string());
  DCHECK(content_setting == CONTENT_SETTING_BLOCK ||
         content_setting == CONTENT_SETTING_ALLOW);
  return content_setting;
}

void PeriodicBackgroundSyncPermissionContext::DecidePermission(
    content::WebContents* web_contents,
    const permissions::PermissionRequestID& id,
    const GURL& requesting_origin,
    const GURL& embedding_origin,
    bool user_gesture,
    permissions::BrowserPermissionCallback callback) {
  // The user should never be prompted to authorize Periodic Background Sync
  // from PeriodicBackgroundSyncPermissionContext.
  NOTREACHED();
}

void PeriodicBackgroundSyncPermissionContext::NotifyPermissionSet(
    const permissions::PermissionRequestID& id,
    const GURL& requesting_origin,
    const GURL& embedding_origin,
    permissions::BrowserPermissionCallback callback,
    bool persist,
    ContentSetting content_setting) {
  DCHECK(!persist);
  permissions::PermissionContextBase::NotifyPermissionSet(
      id, requesting_origin, embedding_origin, std::move(callback), persist,
      content_setting);
}
