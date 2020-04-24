// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_MAC_XPC_SERVICE_NAMES_H_
#define CHROME_UPDATER_MAC_XPC_SERVICE_NAMES_H_

#include "base/mac/scoped_cftyperef.h"
#include "base/mac/scoped_nsobject.h"

namespace updater {
base::ScopedCFTypeRef<CFStringRef> CopyGoogleUpdateCheckLaunchDName();
base::ScopedCFTypeRef<CFStringRef> CopyGoogleUpdateServiceLaunchDName();
base::scoped_nsobject<NSString> GetGoogleUpdateCheckLaunchDLabel();
base::scoped_nsobject<NSString> GetGoogleUpdateServiceLaunchDLabel();
base::scoped_nsobject<NSString> GetGoogleUpdateServiceMachName(NSString* name);
base::scoped_nsobject<NSString> GetGoogleUpdateServiceMachName();
}  // namespace updater

#endif  // CHROME_UPDATER_MAC_XPC_SERVICE_NAMES_H_
