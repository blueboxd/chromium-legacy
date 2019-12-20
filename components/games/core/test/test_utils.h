// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_GAMES_CORE_TEST_TEST_UTILS_H_
#define COMPONENTS_GAMES_CORE_TEST_TEST_UTILS_H_

#include <vector>

#include "base/time/time.h"
#include "components/games/core/proto/date.pb.h"
#include "components/games/core/proto/game.pb.h"
#include "components/games/core/proto/game_image.pb.h"
#include "components/games/core/proto/games_catalog.pb.h"
#include "components/games/core/proto/highlighted_games.pb.h"

namespace games {
namespace test {

GamesCatalog CreateGamesCatalog(std::vector<Game> games);

GamesCatalog CreateGamesCatalogWithOneGame();

GamesCatalog CreateCatalogWithTwoGames();

Game CreateGame(int id = 1);

Date CreateDate(int year, int month, int day);

HighlightedGamesResponse CreateHighlightedGamesResponse();

void ExpectProtosEqual(const google::protobuf::MessageLite& expected,
                       const google::protobuf::MessageLite& actual);

void SetDateProtoTo(const base::Time& time, Date* date_proto);

}  // namespace test
}  // namespace games

#endif  // COMPONENTS_GAMES_CORE_TEST_TEST_UTILS_H_
