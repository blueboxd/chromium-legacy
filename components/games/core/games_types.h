// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_GAMES_CORE_GAMES_TYPES_H_
#define COMPONENTS_GAMES_CORE_GAMES_TYPES_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "components/games/core/proto/game.pb.h"
#include "components/games/core/proto/games_catalog.pb.h"

namespace games {

enum ResponseCode {
  kSuccess = 0,
  kFileNotFound = 1,
  kInvalidData = 2,
  kMissingCatalog = 3,
  kComponentNotInstalled = 4,
};

using GamesCatalogCallback =
    base::OnceCallback<void(ResponseCode, std::unique_ptr<GamesCatalog>)>;
using HighlightedGameCallback =
    base::OnceCallback<void(ResponseCode, const Game)>;

}  // namespace games

#endif  // COMPONENTS_GAMES_CORE_GAMES_TYPES_H_
