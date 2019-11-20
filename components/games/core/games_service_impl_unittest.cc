// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "components/games/core/games_service_impl.h"

#include <memory>
#include <string>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/strings/string_piece.h"
#include "base/test/bind_test_util.h"
#include "components/games/core/games_prefs.h"
#include "components/games/core/games_types.h"
#include "components/games/core/proto/game.pb.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace games {

class GamesServiceImplTest : public testing::Test {
 protected:
  void SetUp() override {
    test_pref_service_ = std::make_unique<TestingPrefServiceSimple>();

    games::prefs::RegisterProfilePrefs(test_pref_service_->registry());

    games_service_ =
        std::make_unique<GamesServiceImpl>(test_pref_service_.get());
  }

  std::unique_ptr<TestingPrefServiceSimple> test_pref_service_;
  std::unique_ptr<GamesServiceImpl> games_service_;
};

TEST_F(GamesServiceImplTest, GetHighlightedGame_SameTitle) {
  prefs::SetInstallDirPath(test_pref_service_.get(),
                           base::FilePath(FILE_PATH_LITERAL("some/path")));

  std::string expected_title = "Some Game!";
  std::string result_title = "not the same";

  games_service_->GetHighlightedGame(base::BindLambdaForTesting(
      [&](ResponseCode code, std::unique_ptr<Game> given_game) {
        ASSERT_EQ(ResponseCode::kSuccess, code);
        ASSERT_NE(nullptr, given_game);
        result_title = given_game->title();
      }));

  ASSERT_EQ(expected_title, result_title);
}

TEST_F(GamesServiceImplTest, MissingFilePaths_FileNotFoundError) {
  bool callback_called = false;

  games_service_->GetHighlightedGame(base::BindLambdaForTesting(
      [&](ResponseCode code, std::unique_ptr<Game> given_game) {
        ASSERT_EQ(ResponseCode::kFileNotFound, code);
        callback_called = true;
      }));

  ASSERT_TRUE(callback_called);
}

}  // namespace games
