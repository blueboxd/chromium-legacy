// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERMISSIONS_CHROME_PERMISSIONS_CLIENT_H_
#define CHROME_BROWSER_PERMISSIONS_CHROME_PERMISSIONS_CLIENT_H_

#include "base/no_destructor.h"
#include "components/permissions/permissions_client.h"

class ChromePermissionsClient : public permissions::PermissionsClient {
 public:
  static ChromePermissionsClient* GetInstance();

  // PermissionsClient:
  HostContentSettingsMap* GetSettingsMap(
      content::BrowserContext* browser_context) override;
  permissions::PermissionDecisionAutoBlocker* GetPermissionDecisionAutoBlocker(
      content::BrowserContext* browser_context) override;
  double GetSiteEngagementScore(content::BrowserContext* browser_context,
                                const GURL& origin) override;
  void GetUkmSourceId(content::BrowserContext* browser_context,
                      const content::WebContents* web_contents,
                      const GURL& requesting_origin,
                      GetUkmSourceIdCallback callback) override;
  permissions::PermissionRequest::IconId GetOverrideIconId(
      ContentSettingsType type) override;
  std::unique_ptr<permissions::NotificationPermissionUiSelector>
  CreateNotificationPermissionUiSelector(
      content::BrowserContext* browser_context) override;
  void OnPromptResolved(content::BrowserContext* browser_context,
                        permissions::PermissionRequestType request_type,
                        permissions::PermissionAction action) override;
  base::Optional<url::Origin> GetAutoApprovalOrigin() override;

 private:
  friend base::NoDestructor<ChromePermissionsClient>;

  ChromePermissionsClient() = default;

  ChromePermissionsClient(const ChromePermissionsClient&) = delete;
  ChromePermissionsClient& operator=(const ChromePermissionsClient&) = delete;
};

#endif  // CHROME_BROWSER_PERMISSIONS_CHROME_PERMISSIONS_CLIENT_H_
