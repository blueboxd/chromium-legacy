// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_BROWSER_PERMISSIONS_WEBLAYER_PERMISSIONS_CLIENT_H_
#define WEBLAYER_BROWSER_PERMISSIONS_WEBLAYER_PERMISSIONS_CLIENT_H_

#include "base/no_destructor.h"
#include "build/build_config.h"
#include "components/permissions/permissions_client.h"

namespace weblayer {

class WebLayerPermissionsClient : public permissions::PermissionsClient {
 public:
  WebLayerPermissionsClient(const WebLayerPermissionsClient&) = delete;
  WebLayerPermissionsClient& operator=(const WebLayerPermissionsClient&) =
      delete;

  static WebLayerPermissionsClient* GetInstance();

  // PermissionsClient:
  HostContentSettingsMap* GetSettingsMap(
      content::BrowserContext* browser_context) override;
  permissions::PermissionDecisionAutoBlocker* GetPermissionDecisionAutoBlocker(
      content::BrowserContext* browser_context) override;
  permissions::PermissionManager* GetPermissionManager(
      content::BrowserContext* browser_context) override;
  permissions::ChooserContextBase* GetChooserContext(
      content::BrowserContext* browser_context,
      ContentSettingsType type) override;
#if defined(OS_ANDROID)
  int MapToJavaDrawableId(int resource_id) override;
#endif

 private:
  friend base::NoDestructor<WebLayerPermissionsClient>;

  WebLayerPermissionsClient() = default;
};

}  // namespace weblayer

#endif  // WEBLAYER_BROWSER_PERMISSIONS_WEBLAYER_PERMISSIONS_CLIENT_H_
