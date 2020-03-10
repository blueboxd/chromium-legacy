// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/native_file_system_dialogs.h"

#include "components/permissions/permission_util.h"

#if !defined(TOOLKIT_VIEWS)
void ShowNativeFileSystemPermissionDialog(
    const NativeFileSystemPermissionRequestManager::RequestData& request,
    base::OnceCallback<void(permissions::PermissionAction result)> callback,
    content::WebContents* web_contents) {
  // There's no dialog version of this available outside views, run callback as
  // if the dialog was instantly cancelled.
  std::move(callback).Run(permissions::PermissionAction::DISMISSED);
}

void ShowNativeFileSystemRestrictedDirectoryDialog(
    const url::Origin& origin,
    const base::FilePath& path,
    bool is_directory,
    base::OnceCallback<void(
        content::NativeFileSystemPermissionContext::SensitiveDirectoryResult)>
        callback,
    content::WebContents* web_contents) {
  // There's no dialog version of this available outside views, run callback as
  // if the dialog was instantly dismissed.
  std::move(callback).Run(content::NativeFileSystemPermissionContext::
                              SensitiveDirectoryResult::kAbort);
}

void ShowNativeFileSystemDirectoryAccessConfirmationDialog(
    const url::Origin& origin,
    const base::FilePath& path,
    base::OnceCallback<void(permissions::PermissionAction result)> callback,
    content::WebContents* web_contents,
    base::ScopedClosureRunner fullscreen_block) {
  // There's no dialog version of this available outside views, run callback as
  // if the dialog was instantly dismissed.
  std::move(callback).Run(permissions::PermissionAction::DISMISSED);
}

#endif  // !defined(TOOLKIT_VIEWS)
