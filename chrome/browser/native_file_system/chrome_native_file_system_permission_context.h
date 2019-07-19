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
  void ConfirmSensitiveDirectoryAccess(
      const url::Origin& origin,
      const std::vector<base::FilePath>& paths,
      int process_id,
      int frame_id,
      base::OnceCallback<void(SensitiveDirectoryResult)> callback) override;
  void ConfirmDirectoryReadAccess(
      const url::Origin& origin,
      const base::FilePath& path,
      int process_id,
      int frame_id,
      base::OnceCallback<void(PermissionStatus)> callback) override;

  struct Grants {
    Grants();
    ~Grants();
    Grants(Grants&&);
    Grants& operator=(Grants&&);

    std::vector<base::FilePath> file_write_grants;
    std::vector<base::FilePath> directory_write_grants;
  };
  Grants GetPermissionGrants(const url::Origin& origin,
                             int process_id,
                             int frame_id);

  // This method must be called on the UI thread, and calls the callback with a
  // snapshot of the currently granted permissions after looking them up.
  // TODO(https://crbug.com/984769): Eliminate process_id and frame_id from this
  // method when grants stop being scoped to a frame.
  static void GetPermissionGrantsFromUIThread(
      content::BrowserContext* browser_context,
      const url::Origin& origin,
      int process_id,
      int frame_id,
      base::OnceCallback<void(Grants)> callback);

  // Revokes directory read access for the given origin in the given tab.
  void RevokeDirectoryReadGrants(const url::Origin& origin,
                                 int process_id,
                                 int frame_id);
  // Revokes write access for the given origin in the given tab.
  void RevokeWriteGrants(const url::Origin& origin,
                         int process_id,
                         int frame_id);

  // Revokes write access and directory read access for the given origin in the
  // given tab.
  static void RevokeGrantsForOriginAndTabFromUIThread(
      content::BrowserContext* browser_context,
      const url::Origin& origin,
      int process_id,
      int frame_id);

  // RefcountedKeyedService:
  void ShutdownOnUIThread() override;

 private:
  // Destructor is private because this class is refcounted.
  ~ChromeNativeFileSystemPermissionContext() override;

  class PermissionGrantImpl;
  void PermissionGrantDestroyed(PermissionGrantImpl* grant);

  void DidConfirmSensitiveDirectoryAccess(
      const url::Origin& origin,
      const std::vector<base::FilePath>& paths,
      int process_id,
      int frame_id,
      base::OnceCallback<void(SensitiveDirectoryResult)> callback,
      bool should_block);

  SEQUENCE_CHECKER(sequence_checker_);

  // Permission state per origin.
  struct OriginState;
  std::map<url::Origin, OriginState> origins_;

  DISALLOW_COPY_AND_ASSIGN(ChromeNativeFileSystemPermissionContext);
};

#endif  // CHROME_BROWSER_NATIVE_FILE_SYSTEM_CHROME_NATIVE_FILE_SYSTEM_PERMISSION_CONTEXT_H_
