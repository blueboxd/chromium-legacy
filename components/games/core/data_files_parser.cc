// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/games/core/data_files_parser.h"

#include "base/files/file_util.h"
#include "components/games/core/games_utils.h"

namespace games {

namespace {

const size_t kMaxFileSizeInBytes = 1024 * 1024;  // 1 MB

bool ParseProtoFromFile(const base::FilePath& file_path,
                        google::protobuf::MessageLite* out_proto) {
  if (!out_proto) {
    return false;
  }

  std::string file_content;
  if (!base::ReadFileToStringWithMaxSize(file_path, &file_content,
                                         kMaxFileSizeInBytes)) {
    return false;
  }

  return out_proto->ParseFromString(file_content);
}

}  //  namespace

DataFilesParser::DataFilesParser() {}

DataFilesParser::~DataFilesParser() = default;

bool DataFilesParser::TryParseCatalog(const base::FilePath& install_dir,
                                      GamesCatalog* out_catalog) {
  return ParseProtoFromFile(GetGamesCatalogPath(install_dir), out_catalog);
}

bool DataFilesParser::TryParseHighlightedGames(
    const base::FilePath& install_dir,
    HighlightedGamesResponse* out_response) {
  return ParseProtoFromFile(GetHighlightedGamesPath(install_dir), out_response);
}

}  // namespace games
