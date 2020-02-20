// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/background_sync/periodic_background_sync_permission_context.h"

#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/components/web_app_helpers.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"

#if defined(OS_ANDROID)
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/android/chrome_jni_headers/BackgroundSyncPwaDetector_jni.h"
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
    const GURL& url) const {
#if defined(OS_ANDROID)
  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jstring> java_url =
      base::android::ConvertUTF8ToJavaString(env, url.spec());
  return Java_BackgroundSyncPwaDetector_isPwaInstalled(env, java_url);
#else
  return web_app::FindInstalledAppWithUrlInScope(
             Profile::FromBrowserContext(browser_context()), url)
      .has_value();
#endif
}

#if defined(OS_ANDROID)
bool PeriodicBackgroundSyncPermissionContext::IsTwaInstalled(
    const GURL& url) const {
  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jstring> java_url =
      base::android::ConvertUTF8ToJavaString(env, url.spec());
  return Java_BackgroundSyncPwaDetector_isTwaInstalled(env, java_url);
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
