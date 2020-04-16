// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UPBOARDING_QUERY_TILES_INTERNAL_QUERY_TILE_STORE_H_
#define CHROME_BROWSER_UPBOARDING_QUERY_TILES_INTERNAL_QUERY_TILE_STORE_H_

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/upboarding/query_tiles/internal/store.h"
#include "chrome/browser/upboarding/query_tiles/internal/tile_group.h"
#include "chrome/browser/upboarding/query_tiles/proto/query_tile_entry.pb.h"
#include "chrome/browser/upboarding/query_tiles/query_tile_entry.h"
#include "components/leveldb_proto/public/proto_database.h"

namespace leveldb_proto {

void DataToProto(upboarding::TileGroup* data,
                 upboarding::query_tiles::proto::QueryTileGroup* proto);
void ProtoToData(upboarding::query_tiles::proto::QueryTileGroup* proto,
                 upboarding::TileGroup* data);

}  // namespace leveldb_proto

namespace upboarding {
// QueryTileStore is the storage layer of all TileGroup which contains
// the top-level tile entries and group metadata. Sub-level tiles are
// recursively owned by their parents.
class QueryTileStore : public Store<TileGroup> {
 public:
  using QueryTileProtoDb = std::unique_ptr<
      leveldb_proto::ProtoDatabase<query_tiles::proto::QueryTileGroup,
                                   TileGroup>>;
  explicit QueryTileStore(QueryTileProtoDb db);
  ~QueryTileStore() override;

  QueryTileStore(const QueryTileStore& other) = delete;
  QueryTileStore& operator=(const QueryTileStore& other) = delete;

 private:
  using KeyEntryVector = std::vector<std::pair<std::string, TileGroup>>;
  using KeyVector = std::vector<std::string>;
  using EntryVector = std::vector<TileGroup>;

  // Store<TileGroup> implementation.
  void InitAndLoad(LoadCallback callback) override;
  void Update(const std::string& key,
              const TileGroup& entry,
              UpdateCallback callback) override;
  void Delete(const std::string& key, DeleteCallback callback) override;

  // Called when db is initialized.
  void OnDbInitialized(LoadCallback callback,
                       leveldb_proto::Enums::InitStatus status);

  // Called when keys and entries are loaded from db.
  void OnDataLoaded(
      LoadCallback callback,
      bool success,
      std::unique_ptr<std::map<std::string, TileGroup>> loaded_entries);

  QueryTileProtoDb db_;

  base::WeakPtrFactory<QueryTileStore> weak_ptr_factory_{this};
};

}  // namespace upboarding

#endif  // CHROME_BROWSER_UPBOARDING_QUERY_TILES_INTERNAL_QUERY_TILE_STORE_H_
