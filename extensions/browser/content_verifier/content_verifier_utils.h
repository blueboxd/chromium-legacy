// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef EXTENSIONS_BROWSER_CONTENT_VERIFIER_CONTENT_VERIFIER_UTILS_H_
#define EXTENSIONS_BROWSER_CONTENT_VERIFIER_CONTENT_VERIFIER_UTILS_H_

#include "base/files/file_path.h"
#include "build/build_config.h"

namespace extensions {
namespace content_verifier_utils {

// Returns true if |path| ends with (.| )+.
// |out_path| will contain "." and/or " " suffix removed from |path|.
bool TrimDotSpaceSuffix(const base::FilePath::StringType& path,
                        base::FilePath::StringType* out_path);

// Returns true if this system/OS's file access is case sensitive.
constexpr bool IsFileAccessCaseSensitive() {
#if defined(OS_WIN) || defined(OS_MACOSX)
  return false;
#else
  return true;
#endif
}

// Returns true if this system/OS ignores (.| )+ suffix in a filepath while
// accessing the file.
constexpr bool IsDotSpaceFilenameSuffixIgnored() {
#if defined(OS_WIN)
  static_assert(!IsFileAccessCaseSensitive(),
                "DotSpace suffix should only be ignored in case-insensitive"
                "systems");
  return true;
#else
  return false;
#endif
}

// Returns platform specific canonicalized version of |relative_path| for
// content verification system.
base::FilePath::StringType CanonicalizeRelativePath(
    const base::FilePath& relative_path);

}  // namespace content_verifier_utils
}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_CONTENT_VERIFIER_CONTENT_VERIFIER_UTILS_H_
