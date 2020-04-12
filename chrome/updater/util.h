// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_UTIL_H_
#define CHROME_UPDATER_UTIL_H_

#include "base/files/file_path.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"

namespace updater {

// Returns a directory where updater files or its data is stored.
bool GetProductDirectory(base::FilePath* path);

// Initializes logging for an executable.
void InitLogging(const base::FilePath::StringType& filename);

// Functor used by associative containers of strings as a case-insensitive ASCII
// compare. |T| could be std::string or base::string16.
template <typename T>
struct CaseInsensitiveASCIICompare {
 public:
  bool operator()(base::BasicStringPiece<T> x,
                  base::BasicStringPiece<T> y) const {
    return base::CompareCaseInsensitiveASCII(x, y) > 0;
  }
};

}  // namespace updater

#endif  // CHROME_UPDATER_UTIL_H_
