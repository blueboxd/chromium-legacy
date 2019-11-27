// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/games/core/data_files_parser.h"

#include "base/files/file_util.h"
#include "components/games/core/games_utils.h"

namespace games {

namespace {

const size_t kMaxCatalogSizeInBytes = 1024 * 1024;  // 1 MB

}  //  namespace

DataFilesParser::DataFilesParser() {}

DataFilesParser::~DataFilesParser() = default;

bool DataFilesParser::TryParseCatalog(const base::FilePath& install_dir,
                                      GamesCatalog* out_catalog) {
  const auto catalog_path = GetGamesCatalogPath(install_dir);

  std::string file_content;
  if (!base::ReadFileToStringWithMaxSize(catalog_path, &file_content,
                                         kMaxCatalogSizeInBytes)) {
    return false;
  }

  out_catalog->ParseFromString(file_content);
  return true;
}

}  // namespace games
