// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_GAMES_CORE_GAMES_UTILS_H_
#define COMPONENTS_GAMES_CORE_GAMES_UTILS_H_

#include "base/files/file_path.h"
#include "base/optional.h"
#include "components/games/core/games_constants.h"
#include "components/games/core/proto/game.pb.h"
#include "components/games/core/proto/games_catalog.pb.h"

namespace games {

const base::FilePath GetGamesCatalogPath(const base::FilePath& dir);
const base::FilePath GetHighlightedGamesPath(const base::FilePath& dir);

// Tries to find a game with the given |id| in the |catalog|. If no game has
// that ID, then we're returning an empty optional instance.
base::Optional<Game> TryFindGameById(int id, const GamesCatalog& catalog);

}  // namespace games

#endif  // COMPONENTS_GAMES_CORE_GAMES_UTILS_H_
