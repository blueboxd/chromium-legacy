// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_WEBRTC_CAMERA_PAN_TILT_ZOOM_PERMISSION_CONTEXT_H_
#define CHROME_BROWSER_MEDIA_WEBRTC_CAMERA_PAN_TILT_ZOOM_PERMISSION_CONTEXT_H_

#include "base/macros.h"
#include "components/permissions/permission_context_base.h"

// Manage user permissions that only controls camera movement (pan/tilt/zoom).
class CameraPanTiltZoomPermissionContext
    : public permissions::PermissionContextBase {
 public:
  explicit CameraPanTiltZoomPermissionContext(
      content::BrowserContext* browser_context);
  ~CameraPanTiltZoomPermissionContext() override;

  CameraPanTiltZoomPermissionContext(
      const CameraPanTiltZoomPermissionContext&) = delete;
  CameraPanTiltZoomPermissionContext& operator=(
      const CameraPanTiltZoomPermissionContext&) = delete;

 private:
  // PermissionContextBase
  void DecidePermission(
      content::WebContents* web_contents,
      const permissions::PermissionRequestID& id,
      const GURL& requesting_origin,
      const GURL& embedding_origin,
      bool user_gesture,
      permissions::BrowserPermissionCallback callback) override;
  bool IsRestrictedToSecureOrigins() const override;
};

#endif  // CHROME_BROWSER_MEDIA_WEBRTC_CAMERA_PAN_TILT_ZOOM_PERMISSION_CONTEXT_H_
