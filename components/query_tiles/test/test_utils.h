// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_QUERY_TILES_TEST_TEST_UTILS_H_
#define COMPONENTS_QUERY_TILES_TEST_TEST_UTILS_H_

#include <string>
#include <vector>

#include "components/query_tiles/internal/tile_group.h"
#include "components/query_tiles/tile.h"

namespace upboarding {
namespace test {

// Print data in Tile, also with tree represent by adjacent nodes
// key-value[parent id: {children id}] pairs.
std::string DebugString(const Tile* entry);

// Print data in TileGroup.
std::string DebugString(const TileGroup* group);

// Build and reset the TileGroup for test usage.
void ResetTestGroup(TileGroup* group);

// TODO(hesen): Have a better builder with parameters to specify the structure
// of tree.
// Build and reset the Tile for test usage.
void ResetTestEntry(Tile* entry);

// Returns true if all data in two TileGroups are identical.
bool AreTileGroupsIdentical(const TileGroup& lhs, const TileGroup& rhs);

// Returns true if all data in two TileEntries are identical.
bool AreTilesIdentical(const Tile& lhs, const Tile& rhs);

// Returns true if all data in two lists of Tile are identical.
bool AreTilesIdentical(std::vector<Tile*> lhs, std::vector<Tile*> rhs);

// Returns true if all data in two lists of Tile are identical.
bool AreTilesIdentical(std::vector<Tile> lhs, std::vector<Tile> rhs);

}  // namespace test
}  // namespace upboarding

#endif  // COMPONENTS_QUERY_TILES_TEST_TEST_UTILS_H_
