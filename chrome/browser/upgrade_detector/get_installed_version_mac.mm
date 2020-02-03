// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/upgrade_detector/get_installed_version.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/mac/keystone_glue.h"

InstalledAndCriticalVersion GetInstalledVersion() {
  return InstalledAndCriticalVersion(base::Version(
      base::UTF16ToASCII(keystone_glue::CurrentlyInstalledVersion())));
}
