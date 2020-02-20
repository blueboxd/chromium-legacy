// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CLIPBOARD_CLIPBOARD_SANITIZED_WRITE_PERMISSION_CONTEXT_H_
#define CHROME_BROWSER_CLIPBOARD_CLIPBOARD_SANITIZED_WRITE_PERMISSION_CONTEXT_H_

#include "base/macros.h"
#include "components/permissions/permission_context_base.h"

// Manages Clipboard API user permissions, for sanitized write only.
class ClipboardSanitizedWritePermissionContext
    : public permissions::PermissionContextBase {
 public:
  explicit ClipboardSanitizedWritePermissionContext(
      content::BrowserContext* browser_context);
  ~ClipboardSanitizedWritePermissionContext() override;

  ClipboardSanitizedWritePermissionContext(
      const ClipboardSanitizedWritePermissionContext&) = delete;
  ClipboardSanitizedWritePermissionContext& operator=(
      const ClipboardSanitizedWritePermissionContext&) = delete;

 private:
  // PermissionContextBase:
  ContentSetting GetPermissionStatusInternal(
      content::RenderFrameHost* render_frame_host,
      const GURL& requesting_origin,
      const GURL& embedding_origin) const override;
  bool IsRestrictedToSecureOrigins() const override;
};

#endif  // CHROME_BROWSER_CLIPBOARD_CLIPBOARD_SANITIZED_WRITE_PERMISSION_CONTEXT_H_
