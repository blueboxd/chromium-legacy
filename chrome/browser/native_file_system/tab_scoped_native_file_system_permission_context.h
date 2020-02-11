// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NATIVE_FILE_SYSTEM_TAB_SCOPED_NATIVE_FILE_SYSTEM_PERMISSION_CONTEXT_H_
#define CHROME_BROWSER_NATIVE_FILE_SYSTEM_TAB_SCOPED_NATIVE_FILE_SYSTEM_PERMISSION_CONTEXT_H_

#include <map>
#include <vector>

#include "chrome/browser/native_file_system/chrome_native_file_system_permission_context.h"
#include "third_party/blink/public/mojom/permissions/permission_status.mojom.h"

// Chrome implementation of NativeFileSystemPermissionContext. This concrete
// subclass of ChromeNativeFileSystemPermissionContext implements a permission
// model where permissions are scoped to the tab they are granted in. I.e. each
// tab has its own independent set of permissions.
//
// All methods must be called on the UI thread.
class TabScopedNativeFileSystemPermissionContext
    : public ChromeNativeFileSystemPermissionContext {
 public:
  class WritePermissionGrantImpl
      : public content::NativeFileSystemPermissionGrant {
   public:
    // In the current implementation permission grants are scoped to the frame
    // they are requested in. Within a frame we only want to have one grant per
    // path. The Key struct contains these fields. Keys are comparable so they
    // can be used with sorted containers like std::map and std::set.
    // TODO(https://crbug.com/984769): Eliminate process_id and frame_id and
    // replace usage of this struct with just a file path when grants stop being
    // scoped to a frame.
    struct Key {
      base::FilePath path;
      int process_id = 0;
      int frame_id = 0;

      bool operator==(const Key& rhs) const;
      bool operator<(const Key& rhs) const;
    };

    WritePermissionGrantImpl(
        base::WeakPtr<TabScopedNativeFileSystemPermissionContext> context,
        const url::Origin& origin,
        const Key& key,
        bool is_directory);

    // content::NativeFileSystemPermissionGrant implementation:
    PermissionStatus GetStatus() override;
    void RequestPermission(
        int process_id,
        int frame_id,
        base::OnceCallback<void(PermissionRequestOutcome)> callback) override;

    const url::Origin& origin() const {
      DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
      return origin_;
    }

    bool is_directory() const {
      DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
      return is_directory_;
    }

    const base::FilePath& path() const {
      DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
      return key_.path;
    }

    const Key& key() const {
      DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
      return key_;
    }

    void SetStatus(PermissionStatus status);

   protected:
    ~WritePermissionGrantImpl() override;

   private:
    void OnPermissionRequestComplete(
        base::OnceCallback<void(PermissionRequestOutcome)> callback,
        PermissionRequestOutcome outcome,
        permissions::PermissionAction result);

    SEQUENCE_CHECKER(sequence_checker_);

    base::WeakPtr<TabScopedNativeFileSystemPermissionContext> const context_;
    const url::Origin origin_;
    const Key key_;
    const bool is_directory_;

    // This member should only be updated via SetStatus(), to make sure
    // observers are properly notified about any change in status.
    PermissionStatus status_ = PermissionStatus::ASK;

    DISALLOW_COPY_AND_ASSIGN(WritePermissionGrantImpl);
  };

  explicit TabScopedNativeFileSystemPermissionContext(
      content::BrowserContext* context);
  ~TabScopedNativeFileSystemPermissionContext() override;

  // content::NativeFileSystemPermissionContext:
  scoped_refptr<content::NativeFileSystemPermissionGrant>
  GetReadPermissionGrant(const url::Origin& origin,
                         const base::FilePath& path,
                         bool is_directory,
                         int process_id,
                         int frame_id,
                         UserAction user_action) override;
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
  // Revokes directory read access for the given origin in the given tab.
  void RevokeDirectoryReadGrants(const url::Origin& origin,
                                 int process_id,
                                 int frame_id);
  // Revokes write access for the given origin in the given tab.
  void RevokeWriteGrants(const url::Origin& origin,
                         int process_id,
                         int frame_id);

  void PermissionGrantDestroyed(WritePermissionGrantImpl* grant);

  base::WeakPtr<ChromeNativeFileSystemPermissionContext> GetWeakPtr() override;

  // Permission state per origin.
  struct OriginState;
  std::map<url::Origin, OriginState> origins_;

  base::WeakPtrFactory<TabScopedNativeFileSystemPermissionContext>
      weak_factory_{this};
};

#endif  // CHROME_BROWSER_NATIVE_FILE_SYSTEM_TAB_SCOPED_NATIVE_FILE_SYSTEM_PERMISSION_CONTEXT_H_
