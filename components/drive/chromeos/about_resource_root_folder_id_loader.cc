// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/drive/chromeos/about_resource_root_folder_id_loader.h"

#include <memory>

#include "base/bind.h"
#include "components/drive/chromeos/about_resource_loader.h"
#include "google_apis/drive/drive_api_parser.h"

namespace drive {
namespace internal {

AboutResourceRootFolderIdLoader::AboutResourceRootFolderIdLoader(
    AboutResourceLoader* about_resource_loader)
    : about_resource_loader_(about_resource_loader) {}

AboutResourceRootFolderIdLoader::~AboutResourceRootFolderIdLoader() = default;

void AboutResourceRootFolderIdLoader::GetRootFolderId(
    const RootFolderIdCallback& callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // If we have already read the root folder id, then we can simply return it.
  if (!root_folder_id_.empty()) {
    callback.Run(FILE_ERROR_OK, root_folder_id_);
    return;
  }

  about_resource_loader_->GetAboutResource(
      base::BindRepeating(&AboutResourceRootFolderIdLoader::OnGetAboutResource,
                          weak_ptr_factory_.GetWeakPtr(), callback));
}

void AboutResourceRootFolderIdLoader::OnGetAboutResource(
    const RootFolderIdCallback& callback,
    google_apis::DriveApiErrorCode status,
    std::unique_ptr<google_apis::AboutResource> about_resource) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  FileError error = GDataToFileError(status);

  if (error != FILE_ERROR_OK) {
    callback.Run(error, base::nullopt);
    return;
  }

  DCHECK(about_resource);

  // The root folder id is immutable so we can save it for subsequent calls.
  root_folder_id_ = about_resource->root_folder_id();
  callback.Run(error, root_folder_id_);
}

}  // namespace internal
}  // namespace drive
