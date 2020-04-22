// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UPBOARDING_QUERY_TILES_INTERNAL_PROTO_CONVERSION_H_
#define CHROME_BROWSER_UPBOARDING_QUERY_TILES_INTERNAL_PROTO_CONVERSION_H_

#include "chrome/browser/upboarding/query_tiles/internal/tile_group.h"
#include "chrome/browser/upboarding/query_tiles/proto/tile.pb.h"
#include "chrome/browser/upboarding/query_tiles/tile.h"

namespace upboarding {

// Converts a Tile to proto.
void TileToProto(upboarding::Tile* entry,
                 upboarding::query_tiles::proto::Tile* proto);

// Converts a proto to Tile.
void TileFromProto(upboarding::query_tiles::proto::Tile* proto,
                   upboarding::Tile* entry);

// Converts a TileGroup to proto.
void TileGroupToProto(TileGroup* group,
                      upboarding::query_tiles::proto::TileGroup* proto);

// Converts a proto to TileGroup.
void TileGroupFromProto(upboarding::query_tiles::proto::TileGroup* proto,
                        TileGroup* group);

}  // namespace upboarding

#endif  // CHROME_BROWSER_UPBOARDING_QUERY_TILES_INTERNAL_PROTO_CONVERSION_H_
