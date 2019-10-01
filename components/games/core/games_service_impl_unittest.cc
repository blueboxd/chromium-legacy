// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "components/games/core/games_service_impl.h"

#include <memory>
#include <string>

#include "base/bind.h"
#include "base/test/bind_test_util.h"
#include "components/games/core/games_types.h"
#include "components/games/core/proto/game.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace games {

class GamesServiceImplTest : public testing::Test {
 protected:
  GamesServiceImplTest() {}

  GamesServiceImpl service;
};

TEST_F(GamesServiceImplTest, GetHighlightedGame_SameTitle) {
  std::string expected_title = "Some Game!";
  std::string result_title = "not the same";

  service.GetHighlightedGame(
      base::BindLambdaForTesting([&](std::unique_ptr<Game> given_game) {
        ASSERT_NE(nullptr, given_game);
        result_title = given_game->title();
      }));

  ASSERT_EQ(expected_title, result_title);
}

}  // namespace games
