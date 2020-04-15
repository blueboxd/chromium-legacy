// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/webrtc/camera_pan_tilt_zoom_permission_context.h"

#include "chrome/browser/content_settings/tab_specific_content_settings.h"
#include "components/permissions/permission_request_id.h"

CameraPanTiltZoomPermissionContext::CameraPanTiltZoomPermissionContext(
    content::BrowserContext* browser_context)
    : PermissionContextBase(browser_context,
                            ContentSettingsType::CAMERA_PAN_TILT_ZOOM,
                            blink::mojom::FeaturePolicyFeature::kNotFound) {}

CameraPanTiltZoomPermissionContext::~CameraPanTiltZoomPermissionContext() =
    default;

void CameraPanTiltZoomPermissionContext::DecidePermission(
    content::WebContents* web_contents,
    const permissions::PermissionRequestID& id,
    const GURL& requesting_origin,
    const GURL& embedding_origin,
    bool user_gesture,
    permissions::BrowserPermissionCallback callback) {
  // TODO(crbug.com/934063): Remove when user prompt is implemented.
  NOTREACHED();
}

bool CameraPanTiltZoomPermissionContext::IsRestrictedToSecureOrigins() const {
  return true;
}
