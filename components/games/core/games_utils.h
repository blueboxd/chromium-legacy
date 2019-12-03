// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_GAMES_CORE_GAMES_UTILS_H_
#define COMPONENTS_GAMES_CORE_GAMES_UTILS_H_

#include "components/games/core/games_constants.h"

#include "base/files/file_path.h"

namespace games {

const base::FilePath GetGamesCatalogPath(const base::FilePath& dir);
const base::FilePath GetHighlightedGamesPath(const base::FilePath& dir);

}  // namespace games

#endif  // COMPONENTS_GAMES_CORE_GAMES_UTILS_H_
