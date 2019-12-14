// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/games/core/games_utils.h"

namespace games {

const base::FilePath GetGamesCatalogPath(const base::FilePath& dir) {
  return dir.Append(kGamesCatalogFileName);
}

const base::FilePath GetHighlightedGamesPath(const base::FilePath& dir) {
  return dir.Append(kHighlightedGamesFileName);
}

base::Optional<Game> TryFindGameById(int id, const GamesCatalog& catalog) {
  base::Optional<Game> optional_game;
  for (const Game& game : catalog.games()) {
    if (game.id() == id) {
      optional_game = game;
      break;
    }
  }
  return optional_game;
}

}  // namespace games
