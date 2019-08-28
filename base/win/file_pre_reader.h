// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file defines a function to pre-read a file in order to avoid touching
// the disk when it is subsequently used.

#include "base/base_export.h"

#ifndef BASE_WIN_FILE_PRE_READER_H_
#define BASE_WIN_FILE_PRE_READER_H_

namespace base {

class FilePath;

namespace win {

// Pre-reads |file_path| to avoid touching the disk when the file is actually
// used. |is_executable| specifies whether the file is to be prefetched as
// executable code or as data. Windows treats the file backed pages in RAM
// differently and specifying the wrong one results in two copies in RAM.
BASE_EXPORT void PreReadFile(const FilePath& file_path, bool is_executable);

}  // namespace win
}  // namespace base

#endif  // BASE_WIN_FILE_PRE_READER_H_
