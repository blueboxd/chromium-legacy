// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/permissions/weblayer_permissions_client.h"

#include "weblayer/browser/host_content_settings_map_factory.h"
#include "weblayer/browser/permissions/permission_decision_auto_blocker_factory.h"
#include "weblayer/browser/permissions/permission_manager_factory.h"

#if defined(OS_ANDROID)
#include "weblayer/browser/android/resource_mapper.h"
#endif

namespace weblayer {

// static
WebLayerPermissionsClient* WebLayerPermissionsClient::GetInstance() {
  static base::NoDestructor<WebLayerPermissionsClient> instance;
  return instance.get();
}

HostContentSettingsMap* WebLayerPermissionsClient::GetSettingsMap(
    content::BrowserContext* browser_context) {
  return HostContentSettingsMapFactory::GetForBrowserContext(browser_context);
}

permissions::PermissionDecisionAutoBlocker*
WebLayerPermissionsClient::GetPermissionDecisionAutoBlocker(
    content::BrowserContext* browser_context) {
  return PermissionDecisionAutoBlockerFactory::GetForBrowserContext(
      browser_context);
}

permissions::PermissionManager* WebLayerPermissionsClient::GetPermissionManager(
    content::BrowserContext* browser_context) {
  return PermissionManagerFactory::GetForBrowserContext(browser_context);
}

permissions::ChooserContextBase* WebLayerPermissionsClient::GetChooserContext(
    content::BrowserContext* browser_context,
    ContentSettingsType type) {
  return nullptr;
}

#if defined(OS_ANDROID)
int WebLayerPermissionsClient::MapToJavaDrawableId(int resource_id) {
  return weblayer::MapToJavaDrawableId(resource_id);
}
#endif

}  // namespace weblayer
