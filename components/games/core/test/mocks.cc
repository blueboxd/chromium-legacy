// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/games/core/test/mocks.h"

namespace games {
namespace test {

MockDataFilesParser::MockDataFilesParser() {}
MockDataFilesParser::~MockDataFilesParser() = default;

MockCatalogStore::MockCatalogStore() : CatalogStore(nullptr) {}
MockCatalogStore::~MockCatalogStore() = default;

void MockCatalogStore::set_cached_catalog(const GamesCatalog* catalog) {
  // Copy catalog to then take ownership.
  GamesCatalog copy(*catalog);
  cached_catalog_ = std::make_unique<GamesCatalog>(copy);
}

MockHighlightedGamesStore::MockHighlightedGamesStore()
    : HighlightedGamesStore(nullptr) {}
MockHighlightedGamesStore::~MockHighlightedGamesStore() = default;

}  // namespace test
}  // namespace games
