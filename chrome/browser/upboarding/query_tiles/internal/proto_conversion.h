// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UPBOARDING_QUERY_TILES_INTERNAL_PROTO_CONVERSION_H_
#define CHROME_BROWSER_UPBOARDING_QUERY_TILES_INTERNAL_PROTO_CONVERSION_H_

#include "chrome/browser/upboarding/query_tiles/proto/query_tile_entry.pb.h"
#include "chrome/browser/upboarding/query_tiles/query_tile_entry.h"

namespace upboarding {

// Converts a QueryTileEntry to proto.
void QueryTileEntryToProto(
    upboarding::QueryTileEntry* entry,
    upboarding::query_tiles::proto::QueryTileEntry* proto);

// Converts a proto to QueryTileEntry.
void QueryTileEntryFromProto(
    upboarding::query_tiles::proto::QueryTileEntry* proto,
    upboarding::QueryTileEntry* entry);

}  // namespace upboarding

#endif  // CHROME_BROWSER_UPBOARDING_QUERY_TILES_INTERNAL_PROTO_CONVERSION_H_
