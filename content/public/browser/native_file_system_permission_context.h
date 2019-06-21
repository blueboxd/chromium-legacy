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

  // Returns the permission grant to use for a particular path. This could be a
  // grant that applies to more than just the path passed in, for example if a
  // user has already granted write access to a directory, this method could
  // return that existing grant when figuring the grant to use for a file in
  // that directory.
  virtual scoped_refptr<NativeFileSystemPermissionGrant>
  GetWritePermissionGrant(const url::Origin& origin,
                          const base::FilePath& path,
                          bool is_directory) = 0;

 protected:
  virtual ~NativeFileSystemPermissionContext() = default;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_NATIVE_FILE_SYSTEM_PERMISSION_CONTEXT_H_
