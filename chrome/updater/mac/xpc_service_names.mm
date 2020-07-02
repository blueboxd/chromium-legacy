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

base::ScopedCFTypeRef<CFStringRef> CopyServiceLaunchDName() {
  return base::SysUTF8ToCFStringRef(MAC_BUNDLE_IDENTIFIER_STRING ".service");
}

base::ScopedCFTypeRef<CFStringRef> CopyAdministrationLaunchDName() {
  return base::SysUTF8ToCFStringRef(MAC_BUNDLE_IDENTIFIER_STRING
                                    ".admin." UPDATER_VERSION_STRING);
}

base::scoped_nsobject<NSString> GetServiceLaunchDLabel() {
  return base::scoped_nsobject<NSString>(
      base::mac::CFToNSCast(CopyServiceLaunchDName().release()));
}

base::scoped_nsobject<NSString> GetAdministrationLaunchDLabel() {
  return base::scoped_nsobject<NSString>(
      base::mac::CFToNSCast(CopyAdministrationLaunchDName().release()));
}

base::scoped_nsobject<NSString> GetServiceMachName(NSString* name) {
  return base::scoped_nsobject<NSString>(
      [name stringByAppendingFormat:@".%lu", [name hash]],
      base::scoped_policy::RETAIN);
}

base::scoped_nsobject<NSString> GetServiceMachName() {
  base::scoped_nsobject<NSString> name(
      base::mac::CFToNSCast(CopyServiceLaunchDName().release()));
  return base::scoped_nsobject<NSString>(
      [name stringByAppendingFormat:@".%lu", [GetServiceLaunchDLabel() hash]],
      base::scoped_policy::RETAIN);
}

}  // namespace updater
