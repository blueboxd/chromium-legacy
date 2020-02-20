// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/storage_access_api/storage_access_grant_permission_context.h"

#include "base/bind.h"
#include "base/logging.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_partition.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/feature_policy/feature_policy.mojom.h"

StorageAccessGrantPermissionContext::StorageAccessGrantPermissionContext(
    content::BrowserContext* browser_context)
    : PermissionContextBase(
          browser_context,
          ContentSettingsType::STORAGE_ACCESS,
          blink::mojom::FeaturePolicyFeature::kStorageAccessAPI),
      content_settings_type_(ContentSettingsType::STORAGE_ACCESS) {}

StorageAccessGrantPermissionContext::~StorageAccessGrantPermissionContext() =
    default;

bool StorageAccessGrantPermissionContext::IsRestrictedToSecureOrigins() const {
  // The Storage Access API and associated grants are allowed on insecure
  // origins as well as secure ones.
  return false;
}

void StorageAccessGrantPermissionContext::DecidePermission(
    content::WebContents* web_contents,
    const permissions::PermissionRequestID& id,
    const GURL& requesting_origin,
    const GURL& embedding_origin,
    bool user_gesture,
    permissions::BrowserPermissionCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!user_gesture ||
      !base::FeatureList::IsEnabled(blink::features::kStorageAccessAPI)) {
    std::move(callback).Run(CONTENT_SETTING_BLOCK);
    return;
  }

  // TODO(https://crbug.com/989663): Apply defined logic to either auto grant an
  // ephemeral grant or potentially prompt for access. For now we will just
  // use the default "ask" for the request if it had a user gesture.

  // Show prompt.
  PermissionContextBase::DecidePermission(web_contents, id, requesting_origin,
                                          embedding_origin, user_gesture,
                                          std::move(callback));
}

ContentSetting StorageAccessGrantPermissionContext::GetPermissionStatusInternal(
    content::RenderFrameHost* render_frame_host,
    const GURL& requesting_origin,
    const GURL& embedding_origin) const {
  if (!base::FeatureList::IsEnabled(blink::features::kStorageAccessAPI)) {
    return CONTENT_SETTING_BLOCK;
  }

  return PermissionContextBase::GetPermissionStatusInternal(
      render_frame_host, requesting_origin, embedding_origin);
}

void StorageAccessGrantPermissionContext::NotifyPermissionSet(
    const permissions::PermissionRequestID& id,
    const GURL& requesting_origin,
    const GURL& embedding_origin,
    permissions::BrowserPermissionCallback callback,
    bool persist,
    ContentSetting content_setting) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (!base::FeatureList::IsEnabled(blink::features::kStorageAccessAPI)) {
    return;
  }

  const bool permission_allowed = (content_setting == CONTENT_SETTING_ALLOW);
  UpdateTabContext(id, requesting_origin, permission_allowed);

  if (!permission_allowed) {
    if (content_setting == CONTENT_SETTING_DEFAULT) {
      content_setting = CONTENT_SETTING_ASK;
    }

    std::move(callback).Run(content_setting);
    return;
  }

  // TODO(https://crbug.com/989663): Potentially set time boxed storage access
  // exemption based on current grants and relay populated content settings to
  // the network service. Also persist setting to HostContentSettingsMapFactory
  // as either persistent or in-memory depending on the grant type.
  HostContentSettingsMap* settings_map =
      HostContentSettingsMapFactory::GetForProfile(browser_context());
  DCHECK(settings_map);

  ContentSettingsForOneType grants;
  settings_map->GetSettingsForOneType(ContentSettingsType::STORAGE_ACCESS,
                                      std::string(), &grants);

  // We only want to signal the renderer process once the default storage
  // partition has updated and ack'd the update. This prevents a race where
  // the renderer could initiate a network request based on the response to this
  // request before the access grants have updated in the network service.
  content::BrowserContext::GetDefaultStoragePartition(browser_context())
      ->GetCookieManagerForBrowserProcess()
      ->SetStorageAccessGrantSettings(
          grants, base::BindOnce(std::move(callback), content_setting));
}

void StorageAccessGrantPermissionContext::UpdateContentSetting(
    const GURL& requesting_origin,
    const GURL& embedding_origin,
    ContentSetting content_setting) {
  // We need to notify the network service of content setting updates before we
  // run our callback. As a result we do our updates when we're notified of a
  // permission being set and should not be called here.
  NOTREACHED();
}
