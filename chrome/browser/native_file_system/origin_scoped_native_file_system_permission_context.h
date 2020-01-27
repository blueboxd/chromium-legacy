// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NATIVE_FILE_SYSTEM_ORIGIN_SCOPED_NATIVE_FILE_SYSTEM_PERMISSION_CONTEXT_H_
#define CHROME_BROWSER_NATIVE_FILE_SYSTEM_ORIGIN_SCOPED_NATIVE_FILE_SYSTEM_PERMISSION_CONTEXT_H_

#include <map>
#include <vector>

#include "chrome/browser/native_file_system/chrome_native_file_system_permission_context.h"
#include "third_party/blink/public/mojom/permissions/permission_status.mojom.h"

// Chrome implementation of NativeFileSystemPermissionContext. This concrete
// subclass of ChromeNativeFileSystemPermissionContext implements a permission
// model where permissions are shared across an entire origin. When the last tab
// for an origin is closed all permissions for that origin are revoked.
//
// All methods must be called on the UI thread.
class OriginScopedNativeFileSystemPermissionContext
    : public ChromeNativeFileSystemPermissionContext {
 public:
  explicit OriginScopedNativeFileSystemPermissionContext(
      content::BrowserContext* context);
  ~OriginScopedNativeFileSystemPermissionContext() override;

  // content::NativeFileSystemPermissionContext:
  scoped_refptr<content::NativeFileSystemPermissionGrant>
  GetReadPermissionGrant(const url::Origin& origin,
                         const base::FilePath& path,
                         bool is_directory,
                         int process_id,
                         int frame_id) override;
  scoped_refptr<content::NativeFileSystemPermissionGrant>
  GetWritePermissionGrant(const url::Origin& origin,
                          const base::FilePath& path,
                          bool is_directory,
                          int process_id,
                          int frame_id,
                          UserAction user_action) override;

  // ChromeNativeFileSystemPermissionContext:
  Grants GetPermissionGrants(const url::Origin& origin,
                             int process_id,
                             int frame_id) override;
  void RevokeGrants(const url::Origin& origin,
                    int process_id,
                    int frame_id) override;

 private:
  base::WeakPtr<ChromeNativeFileSystemPermissionContext> GetWeakPtr() override;

  base::WeakPtrFactory<OriginScopedNativeFileSystemPermissionContext>
      weak_factory_{this};
};

#endif  // CHROME_BROWSER_NATIVE_FILE_SYSTEM_ORIGIN_SCOPED_NATIVE_FILE_SYSTEM_PERMISSION_CONTEXT_H_
