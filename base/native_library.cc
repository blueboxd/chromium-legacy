// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/native_library.h"

namespace base {

NativeLibrary LoadNativeLibrary(const FilePath& library_path,
                                NativeLibraryLoadError* error,
                                NativeLibraryOptions options) {
  return LoadNativeLibraryWithOptions(
      library_path, options, error);
}

}  // namespace base
