// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/games/core/games_service_impl.h"

#include <memory>

#include "base/callback.h"
#include "components/games/core/games_types.h"
#include "components/games/core/proto/game.pb.h"

namespace games {

GamesServiceImpl::GamesServiceImpl() {}

GamesServiceImpl::~GamesServiceImpl() = default;

void GamesServiceImpl::GetHighlightedGame(HighlightedGameCallback callback) {
  auto game_proto = std::make_unique<Game>();
  game_proto->set_title("Some Game!");
  std::move(callback).Run(std::move(game_proto));
}

}  // namespace games
