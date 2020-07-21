// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_INSTALL_STAGE_H_
#define EXTENSIONS_BROWSER_INSTALL_STAGE_H_

namespace extensions {

// The different stages of the extension installation process.
enum class InstallationStage {
  // The validation of signature of the extensions is about to be started.
  kVerification = 0,
  // Extension archive is about to be copied into the working directory.
  kCopying = 1,
  // Extension archive is about to be unpacked.
  kUnpacking = 2,
  // Final checks before the installation is finished.
  kFinalizing = 3,
  kComplete = 4,
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_INSTALL_STAGE_H_
