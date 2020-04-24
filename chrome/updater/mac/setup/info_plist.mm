// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/updater/mac/setup/info_plist.h"

#include "base/files/file_path.h"
#include "base/mac/foundation_util.h"
#include "base/strings/sys_string_conversions.h"
#import "chrome/updater/mac/xpc_service_names.h"

namespace updater {

InfoPlist::InfoPlist(const base::FilePath& info_plist_path)
    : info_plist_(
          [NSDictionary dictionaryWithContentsOfFile:
                            base::mac::FilePathToNSString(info_plist_path)],
          base::scoped_policy::RETAIN),
      bundle_version_(base::SysNSStringToUTF8([info_plist_
          valueForKey:base::mac::CFToNSCast(kCFBundleVersionKey)])),
      valid_(info_plist_ != nil && !bundle_version_.empty()) {}
InfoPlist::~InfoPlist() {}

bool InfoPlist::Valid() {
  return valid_;
}

base::FilePath InfoPlist::UpdaterVersionedFolderPath(
    const base::FilePath& updater_folder_path) const {
  return valid_ ? updater_folder_path.Append(bundle_version_)
                : base::FilePath();
}

base::FilePath InfoPlist::UpdaterExecutablePath(
    const base::FilePath& library_folder_path,
    const base::FilePath& update_folder_name,
    const base::FilePath& updater_app_name,
    const base::FilePath& updater_app_executable_path) const {
  return valid_ ? library_folder_path.Append(update_folder_name)
                      .Append(bundle_version_)
                      .Append(updater_app_name)
                      .Append(updater_app_executable_path)
                : base::FilePath();
}

base::ScopedCFTypeRef<CFStringRef>
InfoPlist::GoogleUpdateCheckLaunchdNameVersioned() const {
  return valid_ ? base::ScopedCFTypeRef<CFStringRef>(CFStringCreateWithFormat(
                      kCFAllocatorDefault, nullptr, CFSTR("%@.%s"),
                      CopyGoogleUpdateCheckLaunchDName().get(),
                      bundle_version_.c_str()))
                : base::ScopedCFTypeRef<CFStringRef>(CFSTR(""));
}

base::ScopedCFTypeRef<CFStringRef>
InfoPlist::GoogleUpdateServiceLaunchdNameVersioned() const {
  return valid_ ? base::ScopedCFTypeRef<CFStringRef>(CFStringCreateWithFormat(
                      kCFAllocatorDefault, nullptr, CFSTR("%@.%s"),
                      CopyGoogleUpdateServiceLaunchDName().get(),
                      bundle_version_.c_str()))
                : base::ScopedCFTypeRef<CFStringRef>(CFSTR(""));
}

}  // namespace updater
