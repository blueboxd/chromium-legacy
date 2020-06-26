// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/lacros/select_file_impl.h"

#include <utility>
#include <vector>

#include "base/files/file_path.h"
#include "chromeos/lacros/mojom/select_file.mojom.h"

namespace chromeos {

// TODO(jamescook): Connection error handling.
SelectFileImpl::SelectFileImpl(
    mojo::PendingReceiver<lacros::mojom::SelectFile> receiver)
    : receiver_(this, std::move(receiver)) {}

SelectFileImpl::~SelectFileImpl() = default;

void SelectFileImpl::Select(lacros::mojom::SelectFileOptionsPtr options,
                            SelectCallback callback) {
  // TODO(jamescook): Open a real select file dialog. For now, just pretend we
  // selected a well-known existing file.
  lacros::mojom::SelectedFileInfoPtr file =
      lacros::mojom::SelectedFileInfo::New();
  file->file_path = base::FilePath("/etc/lsb-release");
  std::vector<lacros::mojom::SelectedFileInfoPtr> files;
  files.push_back(std::move(file));
  std::move(callback).Run(lacros::mojom::SelectFileResult::kSuccess,
                          std::move(files));
}

}  // namespace chromeos
