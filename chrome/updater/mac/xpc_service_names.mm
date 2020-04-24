// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/updater/mac/xpc_service_names.h"

#include "base/files/file_path.h"
#include "base/mac/foundation_util.h"
#include "base/strings/strcat.h"
#include "base/strings/sys_string_conversions.h"
#include "chrome/updater/updater_version.h"

namespace updater {

base::ScopedCFTypeRef<CFStringRef> CopyGoogleUpdateCheckLaunchDName() {
  return base::ScopedCFTypeRef<CFStringRef>(
      base::SysUTF8ToCFStringRef(base::StrCat({
          MAC_BUNDLE_IDENTIFIER_STRING,
          ".check",
      })),
      base::scoped_policy::RETAIN);
}

base::ScopedCFTypeRef<CFStringRef> CopyGoogleUpdateServiceLaunchDName() {
  return base::ScopedCFTypeRef<CFStringRef>(
      base::SysUTF8ToCFStringRef(base::StrCat({
          MAC_BUNDLE_IDENTIFIER_STRING,
          ".service",
      })),
      base::scoped_policy::RETAIN);
}

base::scoped_nsobject<NSString> GetGoogleUpdateCheckLaunchDLabel() {
  return base::scoped_nsobject<NSString>(
      base::mac::CFToNSCast(CopyGoogleUpdateCheckLaunchDName()));
}

base::scoped_nsobject<NSString> GetGoogleUpdateServiceLaunchDLabel() {
  return base::scoped_nsobject<NSString>(
      base::mac::CFToNSCast(CopyGoogleUpdateServiceLaunchDName()));
}

base::scoped_nsobject<NSString> GetGoogleUpdateServiceMachName(NSString* name) {
  return base::scoped_nsobject<NSString>(
      [name stringByAppendingFormat:@".%lu", [name hash]],
      base::scoped_policy::RETAIN);
}

base::scoped_nsobject<NSString> GetGoogleUpdateServiceMachName() {
  base::scoped_nsobject<NSString> name(
      base::mac::CFToNSCast(CopyGoogleUpdateServiceLaunchDName()));
  return base::scoped_nsobject<NSString>(
      [name
          stringByAppendingFormat:@".%lu",
                                  [GetGoogleUpdateServiceLaunchDLabel() hash]],
      base::scoped_policy::RETAIN);
}

}  // namespace updater
