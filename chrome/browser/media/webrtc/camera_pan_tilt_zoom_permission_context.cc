// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/webrtc/camera_pan_tilt_zoom_permission_context.h"

#include "components/permissions/permission_request_id.h"
#include "components/permissions/permissions_client.h"
#include "third_party/blink/public/mojom/feature_policy/feature_policy_feature.mojom-shared.h"

CameraPanTiltZoomPermissionContext::CameraPanTiltZoomPermissionContext(
    content::BrowserContext* browser_context)
    : PermissionContextBase(browser_context,
                            ContentSettingsType::CAMERA_PAN_TILT_ZOOM,
                            blink::mojom::FeaturePolicyFeature::kNotFound) {
  host_content_settings_map_ =
      permissions::PermissionsClient::Get()->GetSettingsMap(browser_context);
  host_content_settings_map_->AddObserver(this);
}

CameraPanTiltZoomPermissionContext::~CameraPanTiltZoomPermissionContext() {
  host_content_settings_map_->RemoveObserver(this);
}

#if defined(OS_ANDROID)
void CameraPanTiltZoomPermissionContext::DecidePermission(
    content::WebContents* web_contents,
    const permissions::PermissionRequestID& id,
    const GURL& requesting_origin,
    const GURL& embedding_origin,
    bool user_gesture,
    permissions::BrowserPermissionCallback callback) {
  // User should not be prompted on Android.
  NOTREACHED();
}
#endif

void CameraPanTiltZoomPermissionContext::OnContentSettingChanged(
    const ContentSettingsPattern& primary_pattern,
    const ContentSettingsPattern& secondary_pattern,
    ContentSettingsType content_type,
    const std::string& resource_identifier) {
  if (content_type != ContentSettingsType::MEDIASTREAM_CAMERA)
    return;

  // TODO(crbug.com/1078272): We should not need to deduce the url from the
  // primary pattern here. Modify the infrastructure to facilitate this
  // particular use case better.
  const GURL url(primary_pattern.ToString());
  if (url::Origin::Create(url).opaque())
    return;

  ContentSetting camera_ptz_setting =
      host_content_settings_map_->GetContentSetting(
          url, url, content_settings_type(), resource_identifier);
  // Don't reset camera PTZ permission if it is already blocked or in a
  // "default" state.
  if (camera_ptz_setting == CONTENT_SETTING_BLOCK ||
      camera_ptz_setting == CONTENT_SETTING_ASK) {
    return;
  }

  ContentSetting mediastream_camera_setting =
      host_content_settings_map_->GetContentSetting(url, url, content_type,
                                                    resource_identifier);
  if (mediastream_camera_setting == CONTENT_SETTING_BLOCK ||
      mediastream_camera_setting == CONTENT_SETTING_ASK) {
    // Automatically reset camera PTZ permission if camera permission
    // gets blocked or reset.
    host_content_settings_map_->SetContentSettingCustomScope(
        primary_pattern, secondary_pattern,
        ContentSettingsType::CAMERA_PAN_TILT_ZOOM, resource_identifier,
        CONTENT_SETTING_DEFAULT);
  }
}

bool CameraPanTiltZoomPermissionContext::IsRestrictedToSecureOrigins() const {
  return true;
}
