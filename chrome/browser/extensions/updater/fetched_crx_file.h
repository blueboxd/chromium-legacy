// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_UPDATER_FETCHED_CRX_FILE_H_
#define CHROME_BROWSER_EXTENSIONS_UPDATER_FETCHED_CRX_FILE_H_

#include <set>

#include "extensions/browser/crx_file_info.h"
#include "extensions/browser/updater/extension_downloader_delegate.h"

namespace extensions {

// FetchedCRXFile holds information about a CRX file we fetched to disk,
// but have not yet unpacked or installed.
struct FetchedCRXFile {
  FetchedCRXFile();
  FetchedCRXFile(const CRXFileInfo& file,
                 bool file_ownership_passed,
                 const std::set<int>& request_ids,
                 ExtensionDownloaderDelegate::InstallCallback callback);
  FetchedCRXFile(FetchedCRXFile&& other);
  FetchedCRXFile& operator=(FetchedCRXFile&&);
  ~FetchedCRXFile();

  FetchedCRXFile(const FetchedCRXFile&) = delete;
  FetchedCRXFile& operator=(const FetchedCRXFile&) = delete;

  CRXFileInfo info;
  GURL download_url;
  bool file_ownership_passed = true;
  std::set<int> request_ids;
  ExtensionDownloaderDelegate::InstallCallback callback;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_UPDATER_FETCHED_CRX_FILE_H_
