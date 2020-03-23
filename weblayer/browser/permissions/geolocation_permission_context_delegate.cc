// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/permissions/geolocation_permission_context_delegate.h"

#include "build/build_config.h"

#if defined(OS_ANDROID)
#include "components/content_settings/core/common/content_settings.h"
#include "components/permissions/permission_util.h"
#include "content/public/browser/web_contents.h"
#include "ui/android/window_android.h"
#include "weblayer/browser/android/permission_request_utils.h"
#include "weblayer/browser/browser_context_impl.h"
#include "weblayer/browser/tab_impl.h"
#endif

namespace weblayer {

bool GeolocationPermissionContextDelegate::DecidePermission(
    content::WebContents* web_contents,
    const permissions::PermissionRequestID& id,
    const GURL& requesting_origin,
    bool user_gesture,
    permissions::BrowserPermissionCallback* callback,
    permissions::GeolocationPermissionContext* context) {
  return false;
}

void GeolocationPermissionContextDelegate::UpdateTabContext(
    const permissions::PermissionRequestID& id,
    const GURL& requesting_frame,
    bool allowed) {}

#if defined(OS_ANDROID)
bool GeolocationPermissionContextDelegate::
    ShouldRequestAndroidLocationPermission(content::WebContents* web_contents) {
  if (!web_contents)
    return false;

  auto* window_android = web_contents->GetTopLevelNativeWindow();
  if (!window_android)
    return false;

  std::vector<std::string> android_permissions;
  permissions::PermissionUtil::GetAndroidPermissionsForContentSetting(
      ContentSettingsType::GEOLOCATION, &android_permissions);

  for (const auto& android_permission : android_permissions) {
    if (!window_android->HasPermission(android_permission))
      return true;
  }
  return false;
}

void GeolocationPermissionContextDelegate::RequestAndroidPermission(
    content::WebContents* web_contents,
    PermissionUpdatedCallback callback) {
  weblayer::RequestAndroidPermission(
      web_contents, ContentSettingsType::GEOLOCATION, std::move(callback));
}

bool GeolocationPermissionContextDelegate::IsInteractable(
    content::WebContents* web_contents) {
  auto* tab = TabImpl::FromWebContents(web_contents);
  return tab && tab->IsActive();
}

PrefService* GeolocationPermissionContextDelegate::GetPrefs(
    content::BrowserContext* browser_context) {
  return static_cast<BrowserContextImpl*>(browser_context)->pref_service();
}

bool GeolocationPermissionContextDelegate::IsRequestingOriginDSE(
    content::BrowserContext* browser_context,
    const GURL& requesting_origin) {
  // TODO(crbug.com/1063433): This may need to be implemented for phase 3.
  return false;
}

void GeolocationPermissionContextDelegate::FinishNotifyPermissionSet(
    const permissions::PermissionRequestID& id,
    const GURL& requesting_origin,
    const GURL& embedding_origin) {}
#endif

}  // namespace weblayer
