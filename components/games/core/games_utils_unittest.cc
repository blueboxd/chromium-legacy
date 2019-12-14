// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/games/core/games_utils.h"

#include "components/games/core/proto/games_catalog.pb.h"
#include "components/games/core/proto/highlighted_games.pb.h"
#include "components/games/core/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace games {

class GamesUtilsTest : public testing::Test {};

TEST_F(GamesUtilsTest, TryFindGameById_NotFound) {
  GamesCatalog catalog = test::CreateCatalogWithTwoGames();
  int notfound_id = -1;

  EXPECT_FALSE(TryFindGameById(notfound_id, catalog));
}

TEST_F(GamesUtilsTest, TryFindGameById_Found) {
  GamesCatalog catalog = test::CreateCatalogWithTwoGames();
  Game expected_game = catalog.games().at(1);

  auto found_game = TryFindGameById(expected_game.id(), catalog);
  ASSERT_TRUE(found_game.has_value());
  test::ExpectProtosEqual(expected_game, found_game.value());
}

}  // namespace games
