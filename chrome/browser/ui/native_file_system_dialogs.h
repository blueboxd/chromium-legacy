// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_NATIVE_FILE_SYSTEM_DIALOGS_H_
#define CHROME_BROWSER_UI_NATIVE_FILE_SYSTEM_DIALOGS_H_

#include <string>
#include <vector>

#include "base/callback.h"
#include "base/callback_helpers.h"
#include "build/build_config.h"
#include "chrome/browser/native_file_system/native_file_system_permission_request_manager.h"
#include "content/public/browser/native_file_system_permission_context.h"

namespace base {
class FilePath;
}

namespace content {
class WebContents;
}

namespace permissions {
enum class PermissionAction;
}

namespace url {
class Origin;
}  // namespace url

// Displays a dialog to ask for write access to the given file or directory for
// the native file system API.
void ShowNativeFileSystemPermissionDialog(
    const NativeFileSystemPermissionRequestManager::RequestData& request,
    base::OnceCallback<void(permissions::PermissionAction result)> callback,
    content::WebContents* web_contents);

// Displays a dialog to inform the user that the |path| they picked using the
// native file system API is blocked by chrome. |is_directory| is true if the
// user was selecting a directory, otherwise the user was selecting files within
// a directory. |callback| is called when the user has dismissed the dialog.
void ShowNativeFileSystemRestrictedDirectoryDialog(
    const url::Origin& origin,
    const base::FilePath& path,
    bool is_directory,
    base::OnceCallback<void(
        content::NativeFileSystemPermissionContext::SensitiveDirectoryResult)>
        callback,
    content::WebContents* web_contents);

// Displays a dialog to confirm that the user intended to give read access to a
// specific directory. Similar to ShowFolderUploadConfirmationDialog above,
// except for use by the Native File System API.
void ShowNativeFileSystemDirectoryAccessConfirmationDialog(
    const url::Origin& origin,
    const base::FilePath& path,
    base::OnceCallback<void(permissions::PermissionAction result)> callback,
    content::WebContents* web_contents,
    base::ScopedClosureRunner fullscreen_block);

#endif  // CHROME_BROWSER_UI_NATIVE_FILE_SYSTEM_DIALOGS_H_
