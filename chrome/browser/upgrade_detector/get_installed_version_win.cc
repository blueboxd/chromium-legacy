// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/upgrade_detector/get_installed_version.h"

#include <utility>

#include "chrome/installer/util/install_util.h"

InstalledAndCriticalVersion GetInstalledVersion() {
  base::Version installed_version =
      InstallUtil::GetChromeVersion(!InstallUtil::IsPerUserInstall());
  if (installed_version.IsValid()) {
    base::Version critical_version = InstallUtil::GetCriticalUpdateVersion();
    if (critical_version.IsValid()) {
      return InstalledAndCriticalVersion(std::move(installed_version),
                                         std::move(critical_version));
    }
  }
  return InstalledAndCriticalVersion(std::move(installed_version));
}
