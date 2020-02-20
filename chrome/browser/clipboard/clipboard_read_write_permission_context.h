// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CLIPBOARD_CLIPBOARD_READ_WRITE_PERMISSION_CONTEXT_H_
#define CHROME_BROWSER_CLIPBOARD_CLIPBOARD_READ_WRITE_PERMISSION_CONTEXT_H_

#include "base/macros.h"
#include "components/permissions/permission_context_base.h"

// Manages Clipboard API user permissions, including unsanitized read and write,
// as well as sanitized read.
class ClipboardReadWritePermissionContext
    : public permissions::PermissionContextBase {
 public:
  explicit ClipboardReadWritePermissionContext(
      content::BrowserContext* browser_context);
  ~ClipboardReadWritePermissionContext() override;

  ClipboardReadWritePermissionContext(
      const ClipboardReadWritePermissionContext&) = delete;
  ClipboardReadWritePermissionContext& operator=(
      const ClipboardReadWritePermissionContext&) = delete;

 private:
  // PermissionContextBase:
  void UpdateTabContext(const permissions::PermissionRequestID& id,
                        const GURL& requesting_frame,
                        bool allowed) override;
  bool IsRestrictedToSecureOrigins() const override;
};

#endif  // CHROME_BROWSER_CLIPBOARD_CLIPBOARD_READ_WRITE_PERMISSION_CONTEXT_H_
