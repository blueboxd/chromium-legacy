// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia/base/scoped_pseudo_file_publisher.h"

#include <lib/vfs/cpp/pseudo_dir.h>
#include <lib/vfs/cpp/pseudo_file.h>

#include "base/fuchsia/fuchsia_logging.h"
#include "base/logging.h"

namespace cr_fuchsia {

ScopedPseudoFilePublisher::ScopedPseudoFilePublisher() = default;

ScopedPseudoFilePublisher::ScopedPseudoFilePublisher(
    vfs::PseudoDir* pseudo_dir,
    base::StringPiece filename,
    std::unique_ptr<vfs::PseudoFile> pseudo_file)
    : pseudo_dir_(pseudo_dir), filename_(filename) {
  DCHECK(pseudo_dir_);

  zx_status_t status = pseudo_dir_->AddEntry(filename_, std::move(pseudo_file));
  ZX_DCHECK(status == ZX_OK, status);
}

ScopedPseudoFilePublisher::~ScopedPseudoFilePublisher() {
  if (pseudo_dir_) {
    zx_status_t status = pseudo_dir_->RemoveEntry(filename_);
    ZX_DCHECK(status == ZX_OK, status);
  }
}

ScopedPseudoFilePublisher::ScopedPseudoFilePublisher(
    ScopedPseudoFilePublisher&& other)
    : pseudo_dir_(other.pseudo_dir_), filename_(std::move(other.filename_)) {
  other.pseudo_dir_ = nullptr;
}

ScopedPseudoFilePublisher& ScopedPseudoFilePublisher::operator=(
    ScopedPseudoFilePublisher&& other) {
  // |tmp| destructor will be called when it gets out of scope, ensuring |this|
  // stops publishing |filename_| if needs be.
  ScopedPseudoFilePublisher tmp(std::move(other));
  std::swap(tmp.pseudo_dir_, pseudo_dir_);
  std::swap(tmp.filename_, filename_);
  return *this;
}

}  // namespace cr_fuchsia
