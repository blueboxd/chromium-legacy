// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERMISSIONS_PERMISSIONS_CLIENT_H_
#define COMPONENTS_PERMISSIONS_PERMISSIONS_CLIENT_H_

#include "base/callback_forward.h"
#include "base/optional.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/permissions/permission_util.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "url/origin.h"

class GURL;
class HostContentSettingsMap;

namespace content {
class BrowserContext;
class WebContents;
}  // namespace content

namespace permissions {
class NotificationPermissionUiSelector;
class PermissionDecisionAutoBlocker;

// Interface to be implemented by permissions embedder to access embedder
// specific logic.
class PermissionsClient {
 public:
  PermissionsClient();
  virtual ~PermissionsClient();

  // Return the permissions client.
  static PermissionsClient* Get();

  // Retrieves the HostContentSettingsMap for this context. The returned pointer
  // has the same lifetime as |browser_context|.
  virtual HostContentSettingsMap* GetSettingsMap(
      content::BrowserContext* browser_context) = 0;

  // Retrieves the PermissionDecisionAutoBlocker for this context. The returned
  // pointer has the same lifetime as |browser_context|.
  virtual PermissionDecisionAutoBlocker* GetPermissionDecisionAutoBlocker(
      content::BrowserContext* browser_context) = 0;

  // Gets the embedder defined engagement score for this |origin|.
  virtual double GetSiteEngagementScore(
      content::BrowserContext* browser_context,
      const GURL& origin);

  // Retrieves the ukm::SourceId (if any) associated with this |browser_context|
  // and |web_contents|. |web_contents| may be null. |callback| will be called
  // with the result, and may be run synchronously if the result is available
  // immediately.
  using GetUkmSourceIdCallback =
      base::OnceCallback<void(base::Optional<ukm::SourceId>)>;
  virtual void GetUkmSourceId(content::BrowserContext* browser_context,
                              const content::WebContents* web_contents,
                              const GURL& requesting_origin,
                              GetUkmSourceIdCallback callback);

  // Returns the icon ID that should be used for permissions UI for |type|. If
  // the embedder returns an empty IconId, the default icon for |type| will be
  // used.
  virtual PermissionRequest::IconId GetOverrideIconId(ContentSettingsType type);

  // Allows the embedder to provide a selector for chossing the UI to use for
  // notification permission requests. If the embedder returns null here, the
  // normal UI will be used.
  virtual std::unique_ptr<NotificationPermissionUiSelector>
  CreateNotificationPermissionUiSelector(
      content::BrowserContext* browser_context);

  // Called for each request type when a permission prompt is resolved.
  virtual void OnPromptResolved(content::BrowserContext* browser_context,
                                PermissionRequestType request_type,
                                PermissionAction action);

  // If the embedder returns an origin here, any requests matching that origin
  // will be approved. Requests that do not match the returned origin will
  // immediately be finished without granting/denying the permission.
  virtual base::Optional<url::Origin> GetAutoApprovalOrigin(
      const PermissionRequest* request);

 private:
  PermissionsClient(const PermissionsClient&) = delete;
  PermissionsClient& operator=(const PermissionsClient&) = delete;
};

}  // namespace permissions

#endif  // COMPONENTS_PERMISSIONS_PERMISSIONS_CLIENT_H_
