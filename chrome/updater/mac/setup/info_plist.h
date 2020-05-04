// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_MAC_SETUP_INFO_PLIST_H_
#define CHROME_UPDATER_MAC_SETUP_INFO_PLIST_H_

#include <memory>

#import "base/mac/scoped_cftyperef.h"
#import "base/mac/scoped_nsobject.h"

namespace base {
class FilePath;
}

namespace updater {

// Extracts the bundle version from the provided Info.plist file and
// constructs various paths based on that version. Used to enable side-by-side
// installs of the updater.
class InfoPlist {
 public:
  // If the file at info_plist_path can't be loaded, returns a nullptr.
  static std::unique_ptr<InfoPlist> Create(
      const base::FilePath& info_plist_path);
  InfoPlist(const InfoPlist&) = delete;
  InfoPlist& operator=(const InfoPlist&) = delete;
  ~InfoPlist();

  base::scoped_nsobject<NSString> BundleVersion() const;
  base::FilePath UpdaterVersionedFolderPath(
      const base::FilePath& updater_folder_path) const;

  base::FilePath UpdaterExecutablePath(
      const base::FilePath& library_folder_path,
      const base::FilePath& update_folder_name,
      const base::FilePath& updater_app_name,
      const base::FilePath& updater_app_executable_path) const;

  base::ScopedCFTypeRef<CFStringRef> GoogleUpdateCheckLaunchdNameVersioned()
      const;

  base::ScopedCFTypeRef<CFStringRef> GoogleUpdateServiceLaunchdNameVersioned()
      const;

 private:
  InfoPlist(base::scoped_nsobject<NSDictionary> info_plist_dictionary,
            base::scoped_nsobject<NSString> bundle_version);

  const base::scoped_nsobject<NSDictionary> info_plist_;
  const std::string bundle_version_;
};

// Gets the path to an Info.plist specified at the bundle path.
base::FilePath InfoPlistPath(const base::FilePath& bundle_path);

// Gets the path to an Info.plist for the current application specified by
// |base::mac::OuterBundlePath()|.
base::FilePath InfoPlistPath();

// Gets the path to local Library directory (~/Library) for the user.
base::FilePath GetLocalLibraryDirectory();

}  // namespace updater

#endif  // CHROME_UPDATER_MAC_SETUP_INFO_PLIST_H_
