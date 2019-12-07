// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/credential_provider/gaiacp/chrome_availability_checker.h"
#include "chrome/credential_provider/gaiacp/gcp_utils.h"
#include "chrome/credential_provider/gaiacp/logging.h"
#include "chrome/installer/launcher_support/chrome_launcher_support.h"

namespace credential_provider {

namespace {

bool IsSupportedChromeVersionInstalled() {
  // Check if Chrome is installed on this machine.
  base::FilePath gls_path =
      chrome_launcher_support::GetChromePathForInstallationLevel(
          chrome_launcher_support::SYSTEM_LEVEL_INSTALLATION, false);
  if (gls_path.empty()) {
    return false;
  }

  // Check if Chrome version is supported.
  base::Version chrome_version =
      chrome_launcher_support::GetChromeVersionForInstallationLevel(
          chrome_launcher_support::SYSTEM_LEVEL_INSTALLATION, false);
  if (!chrome_version.IsValid() ||
      chrome_version < GetMinimumSupportedChromeVersion()) {
    return false;
  }
  return true;
}

}  // namespace

// static
ChromeAvailabilityChecker* ChromeAvailabilityChecker::Get() {
  return *GetInstanceStorage();
}

// static
ChromeAvailabilityChecker** ChromeAvailabilityChecker::GetInstanceStorage() {
  static ChromeAvailabilityChecker instance;
  static ChromeAvailabilityChecker* instance_storage = &instance;

  return &instance_storage;
}

ChromeAvailabilityChecker::ChromeAvailabilityChecker() = default;

ChromeAvailabilityChecker::~ChromeAvailabilityChecker() = default;

bool ChromeAvailabilityChecker::HasSupportedChromeVersion() {
  return IsSupportedChromeVersionInstalled();
}

}  // namespace credential_provider
