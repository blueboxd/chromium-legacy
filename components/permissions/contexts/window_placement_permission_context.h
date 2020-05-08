// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERMISSIONS_CONTEXTS_WINDOW_PLACEMENT_PERMISSION_CONTEXT_H_
#define COMPONENTS_PERMISSIONS_CONTEXTS_WINDOW_PLACEMENT_PERMISSION_CONTEXT_H_

#include "components/permissions/permission_context_base.h"

class WindowPlacementPermissionContext
    : public permissions::PermissionContextBase {
 public:
  explicit WindowPlacementPermissionContext(
      content::BrowserContext* browser_context);
  ~WindowPlacementPermissionContext() override;

  WindowPlacementPermissionContext(const WindowPlacementPermissionContext&) =
      delete;
  WindowPlacementPermissionContext& operator=(
      const WindowPlacementPermissionContext&) = delete;

 protected:
  bool IsRestrictedToSecureOrigins() const override;
};

#endif  // COMPONENTS_PERMISSIONS_CONTEXTS_WINDOW_PLACEMENT_PERMISSION_CONTEXT_H_
