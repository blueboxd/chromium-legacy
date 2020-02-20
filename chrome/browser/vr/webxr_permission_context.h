// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_WEBXR_PERMISSION_CONTEXT_H_
#define CHROME_BROWSER_VR_WEBXR_PERMISSION_CONTEXT_H_

#include "base/macros.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/permissions/permission_context_base.h"

class WebXrPermissionContext : public permissions::PermissionContextBase {
 public:
  WebXrPermissionContext(content::BrowserContext* browser_context,
                         ContentSettingsType content_settings_type);

  ~WebXrPermissionContext() override;

 private:
  // PermissionContextBase:
  bool IsRestrictedToSecureOrigins() const override;

  ContentSettingsType content_settings_type_;

  DISALLOW_COPY_AND_ASSIGN(WebXrPermissionContext);
};

#endif  // CHROME_BROWSER_VR_WEBXR_PERMISSION_CONTEXT_H_
