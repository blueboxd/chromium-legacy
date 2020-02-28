// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_MAC_INSTALLER_H_
#define CHROME_UPDATER_MAC_INSTALLER_H_

#include <string>

namespace base {
class FilePath;
}

namespace updater {

// Mounts the DMG specified by |dmg_file_path|. The install script located at
// "/.install.sh" in the mounted volume is executed, and then the DMG is
// un-mounted. Returns false if mounting the dmg or executing the script failed.
bool InstallFromDMG(const base::FilePath& dmg_file_path,
                    const std::string& arguments);

}  // namespace updater

#endif  // CHROME_UPDATER_MAC_INSTALLER_H_
