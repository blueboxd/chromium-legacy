// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NATIVE_FILE_SYSTEM_CHROME_NATIVE_FILE_SYSTEM_PERMISSION_CONTEXT_H_
#define CHROME_BROWSER_NATIVE_FILE_SYSTEM_CHROME_NATIVE_FILE_SYSTEM_PERMISSION_CONTEXT_H_

#include <map>

#include "base/sequence_checker.h"
#include "components/keyed_service/core/refcounted_keyed_service.h"
#include "content/public/browser/native_file_system_permission_context.h"
#include "third_party/blink/public/mojom/permissions/permission_status.mojom.h"

namespace content {
class BrowserContext;
}  // namespace content

// Chrome implementation of NativeFileSystemPermissionContext. Currently
// implements a single per-origin write permission state.
//
// All methods should be called on the same sequence, except for the
// constructor, destructor, and GetPermissionGrantsFromUIThread method.
//
// TODO(mek): Reconsider if this class should just be UI-thread only, avoiding
// the need to make this ref-counted.
//
// This class does not inherit from ChooserContextBase because the model this
// API uses doesn't really match what ChooserContextBase has to provide. The
// limited lifetime of native file system permission grants (scoped to the
// lifetime of the handles that reference the grants), and the possible
// interactions between grants for directories and grants for children of those
// directories as well as possible interactions between read and write grants
// make it harder to squeeze this into a shape that fits with
// ChooserContextBase.
class ChromeNativeFileSystemPermissionContext
    : public content::NativeFileSystemPermissionContext,
      public RefcountedKeyedService {
 public:
  explicit ChromeNativeFileSystemPermissionContext(
      content::BrowserContext* context);

  // content::NativeFileSystemPermissionContext:
  scoped_refptr<content::NativeFileSystemPermissionGrant>
  GetWritePermissionGrant(const url::Origin& origin,
                          const base::FilePath& path,
                          bool is_directory) override;

  struct Grants {
    Grants();
    ~Grants();
    Grants(Grants&&);
    Grants& operator=(Grants&&);

    std::vector<base::FilePath> file_write_grants;
    std::vector<base::FilePath> directory_write_grants;
  };
  Grants GetPermissionGrants(const url::Origin& origin);

  // This method must be called on the UI thread, and calls the callback with a
  // snapshot of the currently granted permissions after looking them up.
  static void GetPermissionGrantsFromUIThread(
      content::BrowserContext* browser_context,
      const url::Origin& origin,
      base::OnceCallback<void(Grants)> callback);

  // RefcountedKeyedService:
  void ShutdownOnUIThread() override;

 private:
  // Destructor is private because this class is refcounted.
  ~ChromeNativeFileSystemPermissionContext() override;

  class PermissionGrantImpl;
  void PermissionGrantDestroyed(PermissionGrantImpl* grant);

  SEQUENCE_CHECKER(sequence_checker_);

  // Permission state per origin.
  struct OriginState;
  std::map<url::Origin, OriginState> origins_;

  DISALLOW_COPY_AND_ASSIGN(ChromeNativeFileSystemPermissionContext);
};

#endif  // CHROME_BROWSER_NATIVE_FILE_SYSTEM_CHROME_NATIVE_FILE_SYSTEM_PERMISSION_CONTEXT_H_
