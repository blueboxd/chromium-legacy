// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/updater/fetched_crx_file.h"

namespace extensions {

FetchedCRXFile::FetchedCRXFile(
    const CRXFileInfo& file,
    bool file_ownership_passed,
    const std::set<int>& request_ids,
    ExtensionDownloaderDelegate::InstallCallback callback)
    : info(file),
      file_ownership_passed(file_ownership_passed),
      request_ids(request_ids),
      callback(std::move(callback)) {}

FetchedCRXFile::FetchedCRXFile() = default;

FetchedCRXFile::FetchedCRXFile(FetchedCRXFile&&) = default;
FetchedCRXFile& FetchedCRXFile::operator=(FetchedCRXFile&&) = default;

FetchedCRXFile::~FetchedCRXFile() = default;

}  // namespace extensions
