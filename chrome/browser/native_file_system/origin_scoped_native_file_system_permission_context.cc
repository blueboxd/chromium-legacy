// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/native_file_system/origin_scoped_native_file_system_permission_context.h"

namespace {

// TODO(mek): Get rid of this class and implement an actual permission model.
class PermissionGrantImpl : public content::NativeFileSystemPermissionGrant {
 public:
  PermissionGrantImpl() = default;

  // NativeFileSystemPermissionGrant:
  PermissionStatus GetStatus() override { return PermissionStatus::GRANTED; }
  void RequestPermission(
      int process_id,
      int frame_id,
      base::OnceCallback<void(PermissionRequestOutcome)> callback) override {
    std::move(callback).Run(PermissionRequestOutcome::kRequestAborted);
  }

 protected:
  ~PermissionGrantImpl() override = default;
};

}  // namespace

OriginScopedNativeFileSystemPermissionContext::
    OriginScopedNativeFileSystemPermissionContext(
        content::BrowserContext* context)
    : ChromeNativeFileSystemPermissionContext(context) {}

OriginScopedNativeFileSystemPermissionContext::
    ~OriginScopedNativeFileSystemPermissionContext() = default;

scoped_refptr<content::NativeFileSystemPermissionGrant>
OriginScopedNativeFileSystemPermissionContext::GetReadPermissionGrant(
    const url::Origin& origin,
    const base::FilePath& path,
    bool is_directory,
    int process_id,
    int frame_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return base::MakeRefCounted<PermissionGrantImpl>();
}

scoped_refptr<content::NativeFileSystemPermissionGrant>
OriginScopedNativeFileSystemPermissionContext::GetWritePermissionGrant(
    const url::Origin& origin,
    const base::FilePath& path,
    bool is_directory,
    int process_id,
    int frame_id,
    UserAction user_action) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return base::MakeRefCounted<PermissionGrantImpl>();
}

ChromeNativeFileSystemPermissionContext::Grants
OriginScopedNativeFileSystemPermissionContext::GetPermissionGrants(
    const url::Origin& origin,
    int process_id,
    int frame_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  NOTIMPLEMENTED();
  return {};
}

void OriginScopedNativeFileSystemPermissionContext::RevokeGrants(
    const url::Origin& origin,
    int process_id,
    int frame_id) {
  NOTIMPLEMENTED();
}

base::WeakPtr<ChromeNativeFileSystemPermissionContext>
OriginScopedNativeFileSystemPermissionContext::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}
