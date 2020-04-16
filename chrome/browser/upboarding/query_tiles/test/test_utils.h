// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UPBOARDING_QUERY_TILES_TEST_TEST_UTILS_H_
#define CHROME_BROWSER_UPBOARDING_QUERY_TILES_TEST_TEST_UTILS_H_

#include <string>

#include "chrome/browser/upboarding/query_tiles/internal/tile_group.h"
#include "chrome/browser/upboarding/query_tiles/query_tile_entry.h"

namespace upboarding {
namespace test {

// Print data in QueryTileEntry, also with tree represent by adjacent nodes
// key-value[parent id: {children id}] pairs.
std::string DebugString(const QueryTileEntry* entry);

// Print data in TileGroup.
std::string DebugString(const TileGroup* group);

// Build and reset the TileGroup for test usage.
void ResetTestGroup(TileGroup* group);

// Build and reset the TileEntry for test usage.
void ResetTestEntry(QueryTileEntry* entry);

// Returns true if all data in two TileGroups are identical.
bool AreTileGroupsIdentical(const TileGroup& lhs, const TileGroup& rhs);

// Returns true if all data in two QueryTileEntries are identical.
bool AreTilesIdentical(const QueryTileEntry& lhs, const QueryTileEntry& rhs);

}  // namespace test
}  // namespace upboarding

#endif  // CHROME_BROWSER_UPBOARDING_QUERY_TILES_TEST_TEST_UTILS_H_
