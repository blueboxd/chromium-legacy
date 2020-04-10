// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SODA_CONSTANTS_H_
#define COMPONENTS_SODA_CONSTANTS_H_

#include "base/files/file_path.h"

namespace speech {

// Location of the SODA component relative to components directory.
extern const base::FilePath::CharType kSodaInstallationRelativePath[];

// Location of the libsoda binary within the SODA installation directory.
extern const base::FilePath::CharType kSodaBinaryRelativePath[];

// Location of the en_us SODA config file within the SODA installation
// directory. Note: SODA is currently only available in English.
extern const base::FilePath::CharType kSodaConfigFileRelativePath[];

// Get the absolute path of the SODA component directory.
const base::FilePath GetSodaDirectory();

// Get the directory containing the latest version of SODA. In most cases
// there will only be one version of SODA, but it is possible for there to be
// multiple versions if a newer version of SODA was recently downloaded before
// the old version gets cleaned up. Returns an empty path if SODA is not
// installed.
const base::FilePath GetLatestSodaDirectory();

// Get the path to the SODA binary. Returns an empty path if SODA is not
// installed.
const base::FilePath GetSodaBinaryPath();

// Get the path to the dictation.ascii_proto config file used by SODA. Returns
// an empty path if SODA is not installed.
const base::FilePath GetSodaConfigPath();

}  // namespace speech

#endif  // COMPONENTS_SODA_CONSTANTS_H_
