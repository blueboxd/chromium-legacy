// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_NATIVE_FILE_SYSTEM_PERMISSION_CONTEXT_H_
#define CONTENT_PUBLIC_BROWSER_NATIVE_FILE_SYSTEM_PERMISSION_CONTEXT_H_

#include "base/files/file_path.h"
#include "content/public/browser/native_file_system_permission_grant.h"
#include "url/origin.h"

namespace content {

// Entry point to an embedder implemented permission context for the Native File
// System API. Instances of this class can be retrieved via a BrowserContext.
// All these methods should always be called on the same sequence.
class NativeFileSystemPermissionContext {
 public:
  // TODO(mek): Add methods related to read permissions as well, when revoking
  // and re-prompting for those gets implemented.

  // The type of action a user took that resulted in needing a permission grant
  // for a particular path. This is used to signal to the permission context if
  // the path was the result of a "save" operation, which an implementation can
  // use to automatically grant write access to the path.
  enum class UserAction {
    // The path for which a permission grant is requested was the result of a
    // "open" dialog, and as such the grant should probably not start out as
    // granted.
    kOpen,
    // The path for which a permission grant is requested was the result of a
    // "save" dialog, and as such it could make sense to return a grant that
    // immediately allows write access without needing to request it.
    kSave,
  };

  // Returns the permission grant to use for a particular path. This could be a
  // grant that applies to more than just the path passed in, for example if a
  // user has already granted write access to a directory, this method could
  // return that existing grant when figuring the grant to use for a file in
  // that directory.
  virtual scoped_refptr<NativeFileSystemPermissionGrant>
  GetWritePermissionGrant(const url::Origin& origin,
                          const base::FilePath& path,
                          bool is_directory,
                          UserAction user_action) = 0;

  // Displays a dialog to confirm that the user intended to give read access to
  // a specific directory.
  using PermissionStatus = blink::mojom::PermissionStatus;
  virtual void ConfirmDirectoryReadAccess(
      const url::Origin& origin,
      const base::FilePath& path,
      int process_id,
      int frame_id,
      base::OnceCallback<void(PermissionStatus)> callback) = 0;

  enum class SensitiveDirectoryResult {
    kAllowed,   // Access to directory is okay.
    kTryAgain,  // User should pick a different directory.
    kAbort,     // Abandon entirely, as if picking was cancelled.
  };
  // Checks if access to the given |paths| should be allowed or blocked. This is
  // used to implement blocks for certain sensitive directories such as the
  // "Windows" system directory, as well as the root of the "home" directory.
  // Calls |callback| with the result of the check, after potentially showing
  // some UI to the user if the path should not be accessed.
  virtual void ConfirmSensitiveDirectoryAccess(
      const url::Origin& origin,
      const std::vector<base::FilePath>& paths,
      int process_id,
      int frame_id,
      base::OnceCallback<void(SensitiveDirectoryResult)> callback) = 0;

 protected:
  virtual ~NativeFileSystemPermissionContext() = default;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_NATIVE_FILE_SYSTEM_PERMISSION_CONTEXT_H_
