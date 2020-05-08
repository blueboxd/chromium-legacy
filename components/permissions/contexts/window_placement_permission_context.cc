// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/permissions/contexts/window_placement_permission_context.h"

#include "components/content_settings/core/common/content_settings_types.h"
#include "third_party/blink/public/mojom/feature_policy/feature_policy.mojom.h"

WindowPlacementPermissionContext::WindowPlacementPermissionContext(
    content::BrowserContext* browser_context)
    : PermissionContextBase(browser_context,
                            ContentSettingsType::WINDOW_PLACEMENT,
                            blink::mojom::FeaturePolicyFeature::kNotFound) {}

WindowPlacementPermissionContext::~WindowPlacementPermissionContext() = default;

bool WindowPlacementPermissionContext::IsRestrictedToSecureOrigins() const {
  return true;
}
