// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_GAMES_CORE_DATA_FILES_PARSER_H_
#define COMPONENTS_GAMES_CORE_DATA_FILES_PARSER_H_

#include "base/files/file_path.h"
#include "components/games/core/proto/games_catalog.pb.h"
#include "components/games/core/proto/highlighted_games.pb.h"

namespace games {

class DataFilesParser {
 public:
  DataFilesParser();
  virtual ~DataFilesParser();

  virtual bool TryParseCatalog(const base::FilePath& install_dir,
                               GamesCatalog* out_catalog);

  virtual bool TryParseHighlightedGames(const base::FilePath& install_dir,
                                        HighlightedGamesResponse* out_response);

  DISALLOW_COPY_AND_ASSIGN(DataFilesParser);
};

}  // namespace games

#endif  // COMPONENTS_GAMES_CORE_DATA_FILES_PARSER_H_
