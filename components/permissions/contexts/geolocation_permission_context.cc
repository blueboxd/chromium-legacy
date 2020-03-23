// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/permissions/contexts/geolocation_permission_context.h"

#include "base/bind.h"
#include "components/permissions/permission_request_id.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/device_service.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "url/origin.h"

namespace permissions {

GeolocationPermissionContext::GeolocationPermissionContext(
    content::BrowserContext* browser_context,
    std::unique_ptr<Delegate> delegate)
    : PermissionContextBase(browser_context,
                            ContentSettingsType::GEOLOCATION,
                            blink::mojom::FeaturePolicyFeature::kGeolocation),
      delegate_(std::move(delegate)) {}

GeolocationPermissionContext::~GeolocationPermissionContext() = default;

void GeolocationPermissionContext::DecidePermission(
    content::WebContents* web_contents,
    const PermissionRequestID& id,
    const GURL& requesting_origin,
    const GURL& embedding_origin,
    bool user_gesture,
    BrowserPermissionCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (!delegate_->DecidePermission(web_contents, id, requesting_origin,
                                   user_gesture, &callback, this)) {
    DCHECK(callback);
    PermissionContextBase::DecidePermission(web_contents, id, requesting_origin,
                                            embedding_origin, user_gesture,
                                            std::move(callback));
  }
}

void GeolocationPermissionContext::UpdateTabContext(
    const PermissionRequestID& id,
    const GURL& requesting_frame,
    bool allowed) {
  delegate_->UpdateTabContext(id, requesting_frame, allowed);

  if (allowed) {
    GetGeolocationControl()->UserDidOptIntoLocationServices();
  }
}

bool GeolocationPermissionContext::IsRestrictedToSecureOrigins() const {
  return true;
}

device::mojom::GeolocationControl*
GeolocationPermissionContext::GetGeolocationControl() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!geolocation_control_) {
    content::GetDeviceService().BindGeolocationControl(
        geolocation_control_.BindNewPipeAndPassReceiver());
  }
  return geolocation_control_.get();
}

}  // namespace permissions
