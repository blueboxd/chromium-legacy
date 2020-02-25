// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_BASE_SCOPED_PSEUDO_FILE_PUBLISHER_H_
#define FUCHSIA_BASE_SCOPED_PSEUDO_FILE_PUBLISHER_H_

#include <memory>
#include <string>

#include "base/strings/string_piece.h"

namespace vfs {
class PseudoDir;
class PseudoFile;
}  // namespace vfs

namespace cr_fuchsia {

// Links |pseudo_file| at the specified |filename| under the specified
// |pseudo_dir|, and unlinks it when going out of scope.
class ScopedPseudoFilePublisher {
 public:
  ScopedPseudoFilePublisher();
  ScopedPseudoFilePublisher(vfs::PseudoDir* pseudo_dir,
                            base::StringPiece filename,
                            std::unique_ptr<vfs::PseudoFile> pseudo_file);
  ~ScopedPseudoFilePublisher();

  ScopedPseudoFilePublisher(ScopedPseudoFilePublisher&&);
  ScopedPseudoFilePublisher& operator=(ScopedPseudoFilePublisher&&);

  ScopedPseudoFilePublisher(const ScopedPseudoFilePublisher&) = delete;
  ScopedPseudoFilePublisher& operator=(const ScopedPseudoFilePublisher&) =
      delete;

 private:
  vfs::PseudoDir* pseudo_dir_ = nullptr;
  std::string filename_;
};

}  // namespace cr_fuchsia

#endif  // FUCHSIA_BASE_SCOPED_PSEUDO_FILE_PUBLISHER_H_
