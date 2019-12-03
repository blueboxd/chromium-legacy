// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/games/core/test/test_utils.h"

namespace games {
namespace test {

GamesCatalog CreateGamesCatalog(std::vector<Game> games) {
  GamesCatalog catalog;

  for (auto& game : games) {
    catalog.mutable_games()->Add(std::move(game));
  }

  return catalog;
}

GamesCatalog CreateGamesCatalogWithOneGame() {
  return CreateGamesCatalog({CreateGame()});
}

Game CreateGame(int id) {
  Game game;
  game.set_id(id);
  game.set_title("Snake");
  game.set_url("https://www.google.com/some/game");

  GameImage game_image;
  game_image.set_type(GameImageType::GAME_IMAGE_TYPE_CARD);
  game_image.set_url("https://www.google.com/some/card/image");

  game.mutable_images()->Add(std::move(game_image));
  return game;
}

Date CreateDate(int year, int month, int day) {
  Date date;
  date.set_year(year);
  date.set_month(month);
  date.set_day(day);
  return date;
}

HighlightedGamesResponse CreateHighlightedGamesResponse() {
  HighlightedGame highlighted_game;
  highlighted_game.mutable_start_date()->CopyFrom(CreateDate(2019, 10, 10));
  highlighted_game.mutable_end_date()->CopyFrom(CreateDate(2019, 10, 17));
  highlighted_game.set_game_id(1);

  HighlightedGamesResponse highlighted_games;
  highlighted_games.mutable_games()->Add(std::move(highlighted_game));

  return highlighted_games;
}

bool AreProtosEqual(const google::protobuf::MessageLite& lhs,
                    const google::protobuf::MessageLite& rhs) {
  return lhs.SerializeAsString() == rhs.SerializeAsString();
}

}  // namespace test
}  // namespace games
