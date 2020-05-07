// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_PLUGIN_VM_PLUGIN_VM_FILES_H_
#define CHROME_BROWSER_CHROMEOS_PLUGIN_VM_PLUGIN_VM_FILES_H_

#include "base/callback.h"
#include "base/files/file_path.h"
#include "storage/browser/file_system/file_system_url.h"

class Profile;

namespace plugin_vm {

// Ensure default shared dir <cryptohome>/MyFiles/PluginVm exists. Invokes
// |callback| with dir and true if dir is successfully created or already
// exists.
void EnsureDefaultSharedDirExists(
    Profile* profile,
    base::OnceCallback<void(const base::FilePath&, bool)> callback);

// Converts a cracked url to a path inside the VM.
// Returns nullopt if the conversion isn't possible.
base::Optional<std::string> ConvertFileSystemURLToPathInsidePluginVmSharedDir(
    Profile* profile,
    const storage::FileSystemURL& file_system_url);

using LaunchPluginVmAppCallback =
    base::OnceCallback<void(bool success, const std::string& failure_reason)>;

// Launch a Plugin VM App with a given set of files, given as cracked urls in
// the VM. Will start Plugin VM if it is not already running.
void LaunchPluginVmApp(Profile* profile,
                       std::string app_id,
                       std::vector<storage::FileSystemURL> files,
                       LaunchPluginVmAppCallback callback);

}  // namespace plugin_vm

#endif  // CHROME_BROWSER_CHROMEOS_PLUGIN_VM_PLUGIN_VM_FILES_H_
