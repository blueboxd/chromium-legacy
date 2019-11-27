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

bool AreProtosEqual(const google::protobuf::MessageLite& lhs,
                    const google::protobuf::MessageLite& rhs) {
  return lhs.SerializeAsString() == rhs.SerializeAsString();
}

}  // namespace test
}  // namespace games
