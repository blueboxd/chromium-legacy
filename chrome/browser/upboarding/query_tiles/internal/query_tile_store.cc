// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/upboarding/query_tiles/internal/query_tile_store.h"

#include <utility>

#include "chrome/browser/upboarding/query_tiles/internal/proto_conversion.h"

namespace leveldb_proto {

void DataToProto(upboarding::QueryTileEntry* data,
                 upboarding::query_tiles::proto::QueryTileEntry* proto) {
  QueryTileEntryToProto(data, proto);
}

void ProtoToData(upboarding::query_tiles::proto::QueryTileEntry* proto,
                 upboarding::QueryTileEntry* data) {
  QueryTileEntryFromProto(proto, data);
}

}  // namespace leveldb_proto

namespace upboarding {

QueryTileStore::QueryTileStore(QueryTileProtoDb db) : db_(std::move(db)) {}

QueryTileStore::~QueryTileStore() = default;

void QueryTileStore::InitAndLoad(LoadCallback callback) {
  db_->Init(base::BindOnce(&QueryTileStore::OnDbInitialized,
                           weak_ptr_factory_.GetWeakPtr(),
                           std::move(callback)));
}

void QueryTileStore::Update(const std::string& key,
                            const QueryTileEntry& entry,
                            UpdateCallback callback) {
  auto entries_to_save = std::make_unique<KeyEntryVector>();
  QueryTileEntry entry_to_save = entry;
  entries_to_save->emplace_back(key, std::move(entry_to_save));
  db_->UpdateEntries(std::move(entries_to_save),
                     std::make_unique<KeyVector>() /*keys_to_remove*/,
                     std::move(callback));
}

void QueryTileStore::Delete(const std::string& key, DeleteCallback callback) {
  auto keys_to_delete = std::make_unique<KeyVector>();
  keys_to_delete->emplace_back(key);
  db_->UpdateEntries(std::make_unique<KeyEntryVector>() /*entries_to_save*/,
                     std::move(keys_to_delete), std::move(callback));
}

void QueryTileStore::OnDbInitialized(LoadCallback callback,
                                     leveldb_proto::Enums::InitStatus status) {
  if (status != leveldb_proto::Enums::InitStatus::kOK) {
    std::move(callback).Run(false, KeysAndEntries());
    return;
  }

  db_->LoadKeysAndEntries(base::BindOnce(&QueryTileStore::OnDataLoaded,
                                         weak_ptr_factory_.GetWeakPtr(),
                                         std::move(callback)));
}

void QueryTileStore::OnDataLoaded(
    LoadCallback callback,
    bool success,
    std::unique_ptr<std::map<std::string, QueryTileEntry>>
        loaded_keys_and_entries) {
  if (!success || !loaded_keys_and_entries) {
    std::move(callback).Run(success, KeysAndEntries());
    return;
  }

  KeysAndEntries keys_and_entries;
  for (auto& it : *loaded_keys_and_entries) {
    std::unique_ptr<QueryTileEntry> entry =
        std::make_unique<QueryTileEntry>(std::move(it.second));
    keys_and_entries.emplace(it.first, std::move(entry));
  }

  std::move(callback).Run(true, std::move(keys_and_entries));
}

}  // namespace upboarding
