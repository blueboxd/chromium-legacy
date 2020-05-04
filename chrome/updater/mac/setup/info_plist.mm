// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/updater/mac/setup/info_plist.h"

#include "base/files/file_path.h"
#include "base/mac/bundle_locations.h"
#include "base/mac/foundation_util.h"
#include "base/memory/ptr_util.h"
#include "base/strings/sys_string_conversions.h"
#import "chrome/updater/mac/xpc_service_names.h"

namespace updater {

InfoPlist::InfoPlist(base::scoped_nsobject<NSDictionary> info_plist_dictionary,
                     base::scoped_nsobject<NSString> bundle_version)
    : info_plist_(info_plist_dictionary),
      bundle_version_(base::SysNSStringToUTF8(bundle_version)) {}
InfoPlist::~InfoPlist() {}

// static
std::unique_ptr<InfoPlist> InfoPlist::Create(
    const base::FilePath& info_plist_path) {
  base::scoped_nsobject<NSDictionary> info_plist_dictionary(
      [NSDictionary dictionaryWithContentsOfFile:base::mac::FilePathToNSString(
                                                     info_plist_path)],
      base::scoped_policy::RETAIN);
  base::scoped_nsobject<NSString> bundle_version(
      [info_plist_dictionary
          valueForKey:base::mac::CFToNSCast(kCFBundleVersionKey)],
      base::scoped_policy::RETAIN);
  return [bundle_version length] > 0
             ? base::WrapUnique(
                   new InfoPlist(info_plist_dictionary, bundle_version))
             : nullptr;
}

base::scoped_nsobject<NSString> InfoPlist::BundleVersion() const {
  return base::scoped_nsobject<NSString>(
      base::SysUTF8ToNSString(bundle_version_));
}

base::FilePath InfoPlist::UpdaterVersionedFolderPath(
    const base::FilePath& updater_folder_path) const {
  return updater_folder_path.Append(bundle_version_);
}

base::FilePath InfoPlist::UpdaterExecutablePath(
    const base::FilePath& library_folder_path,
    const base::FilePath& update_folder_name,
    const base::FilePath& updater_app_name,
    const base::FilePath& updater_app_executable_path) const {
  return library_folder_path.Append(update_folder_name)
      .Append(bundle_version_)
      .Append(updater_app_name)
      .Append(updater_app_executable_path);
}

base::ScopedCFTypeRef<CFStringRef>
InfoPlist::GoogleUpdateCheckLaunchdNameVersioned() const {
  return base::ScopedCFTypeRef<CFStringRef>(CFStringCreateWithFormat(
      kCFAllocatorDefault, nullptr, CFSTR("%@.%s"),
      CopyGoogleUpdateCheckLaunchDName().get(), bundle_version_.c_str()));
}

base::ScopedCFTypeRef<CFStringRef>
InfoPlist::GoogleUpdateServiceLaunchdNameVersioned() const {
  return base::ScopedCFTypeRef<CFStringRef>(CFStringCreateWithFormat(
      kCFAllocatorDefault, nullptr, CFSTR("%@.%s"),
      CopyGoogleUpdateServiceLaunchDName().get(), bundle_version_.c_str()));
}

base::FilePath InfoPlistPath(const base::FilePath& bundle_path) {
  return bundle_path.Append("Contents").Append("Info.plist");
}

base::FilePath InfoPlistPath() {
  return InfoPlistPath(base::mac::OuterBundlePath());
}

base::FilePath GetLocalLibraryDirectory() {
  base::FilePath local_library_path;
  if (!base::mac::GetLocalDirectory(NSLibraryDirectory, &local_library_path)) {
    VLOG(1) << "Could not get local library path";
  }
  return local_library_path;
}

}  // namespace updater
