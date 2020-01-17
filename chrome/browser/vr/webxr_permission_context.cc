// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/webxr_permission_context.h"

#include "base/logging.h"
#include "third_party/blink/public/mojom/feature_policy/feature_policy.mojom.h"

WebXrPermissionContext::WebXrPermissionContext(
    Profile* profile,
    ContentSettingsType content_settings_type)
    : PermissionContextBase(profile,
                            content_settings_type,
                            blink::mojom::FeaturePolicyFeature::kWebXr),
      content_settings_type_(content_settings_type) {
  DCHECK(content_settings_type_ == ContentSettingsType::VR ||
         content_settings_type_ == ContentSettingsType::AR);
}

WebXrPermissionContext::~WebXrPermissionContext() = default;

bool WebXrPermissionContext::IsRestrictedToSecureOrigins() const {
  return true;
}
