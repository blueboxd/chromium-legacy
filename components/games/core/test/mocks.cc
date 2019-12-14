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
    : HighlightedGamesStore(nullptr, nullptr) {}
MockHighlightedGamesStore::~MockHighlightedGamesStore() = default;

MockClock::MockClock() {}
MockClock::~MockClock() = default;

void MockClock::MockNow(const base::Time& fake_time) {
  mock_now_ = fake_time;
}

base::Time MockClock::Now() const {
  return mock_now_;
}

void MockClock::AdvanceDays(int days) {
  mock_now_ += base::TimeDelta::FromDays(days);
}

}  // namespace test
}  // namespace games
