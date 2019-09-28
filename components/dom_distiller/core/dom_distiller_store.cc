// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/dom_distiller/core/dom_distiller_store.h"

#include <stddef.h>

#include <utility>

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "components/dom_distiller/core/article_entry.h"
#include "components/sync/model/sync_change.h"

using leveldb_proto::ProtoDatabase;
using syncer::SyncChange;
using syncer::SyncChangeList;
using syncer::SyncDataList;

namespace dom_distiller {

DomDistillerStore::DomDistillerStore(
    std::unique_ptr<ProtoDatabase<ArticleEntry>> database)
    : database_(std::move(database)), database_loaded_(false) {
  database_->Init(base::BindOnce(&DomDistillerStore::OnDatabaseInit,
                                 weak_ptr_factory_.GetWeakPtr()));
}

DomDistillerStore::DomDistillerStore(
    std::unique_ptr<ProtoDatabase<ArticleEntry>> database,
    const std::vector<ArticleEntry>& initial_data)
    : database_(std::move(database)),
      database_loaded_(false),
      model_(initial_data) {
  database_->Init(base::BindOnce(&DomDistillerStore::OnDatabaseInit,
                                 weak_ptr_factory_.GetWeakPtr()));
}

DomDistillerStore::~DomDistillerStore() {}

bool DomDistillerStore::GetEntryById(const std::string& entry_id,
                                     ArticleEntry* entry) {
  return model_.GetEntryById(entry_id, entry);
}

bool DomDistillerStore::GetEntryByUrl(const GURL& url, ArticleEntry* entry) {
  return model_.GetEntryByUrl(url, entry);
}

bool DomDistillerStore::AddEntry(const ArticleEntry& entry) {
  return ChangeEntry(entry, SyncChange::ACTION_ADD);
}

bool DomDistillerStore::UpdateEntry(const ArticleEntry& entry) {
  return ChangeEntry(entry, SyncChange::ACTION_UPDATE);
}

bool DomDistillerStore::RemoveEntry(const ArticleEntry& entry) {
  return ChangeEntry(entry, SyncChange::ACTION_DELETE);
}

bool DomDistillerStore::ChangeEntry(const ArticleEntry& entry,
                                    SyncChange::SyncChangeType changeType) {
  if (!database_loaded_) {
    return false;
  }

  bool hasEntry = model_.GetEntryById(entry.entry_id(), nullptr);
  if (hasEntry) {
    if (changeType == SyncChange::ACTION_ADD) {
      DVLOG(1) << "Already have entry with id " << entry.entry_id() << ".";
      return false;
    }
  } else if (changeType != SyncChange::ACTION_ADD) {
    DVLOG(1) << "No entry with id " << entry.entry_id() << " found.";
    return false;
  }

  SyncChangeList changes_to_apply;
  changes_to_apply.push_back(
      SyncChange(FROM_HERE, changeType, CreateLocalData(entry)));

  SyncChangeList changes_applied;
  SyncChangeList changes_missing;

  ApplyChangesToModel(changes_to_apply, &changes_applied, &changes_missing);

  if (changeType == SyncChange::ACTION_UPDATE && changes_applied.size() != 1) {
    DVLOG(1) << "Failed to update entry with id " << entry.entry_id() << ".";
    return false;
  }

  DCHECK_EQ(size_t(0), changes_missing.size());
  DCHECK_EQ(size_t(1), changes_applied.size());

  ApplyChangesToDatabase(changes_applied);

  return true;
}

std::vector<ArticleEntry> DomDistillerStore::GetEntries() const {
  return model_.GetEntries();
}

void DomDistillerStore::ApplyChangesToModel(const SyncChangeList& changes,
                                            SyncChangeList* changes_applied,
                                            SyncChangeList* changes_missing) {
  model_.ApplyChangesToModel(changes, changes_applied, changes_missing);
}

void DomDistillerStore::OnDatabaseInit(
    leveldb_proto::Enums::InitStatus status) {
  if (status != leveldb_proto::Enums::InitStatus::kOK) {
    DVLOG(1) << "DOM Distiller database init failed.";
    database_.reset();
    return;
  }
  database_->LoadEntries(base::BindOnce(&DomDistillerStore::OnDatabaseLoad,
                                        weak_ptr_factory_.GetWeakPtr()));
}

void DomDistillerStore::OnDatabaseLoad(bool success,
                                       std::unique_ptr<EntryVector> entries) {
  if (!success) {
    DVLOG(1) << "DOM Distiller database load failed.";
    database_.reset();
    return;
  }
  database_loaded_ = true;

  SyncDataList data;
  for (auto it = entries->begin(); it != entries->end(); ++it) {
    data.push_back(CreateLocalData(*it));
  }
  SyncChangeList changes_applied;
  SyncChangeList database_changes_needed;
  MergeDataWithModel(data, &changes_applied, &database_changes_needed);
  ApplyChangesToDatabase(database_changes_needed);
}

void DomDistillerStore::OnDatabaseSave(bool success) {
  if (!success) {
    DVLOG(1) << "DOM Distiller database save failed."
             << " Disabling modifications and sync.";
    database_.reset();
    database_loaded_ = false;
  }
}

bool DomDistillerStore::ApplyChangesToDatabase(
    const SyncChangeList& change_list) {
  if (!database_loaded_) {
    return false;
  }
  if (change_list.empty()) {
    return true;
  }
  std::unique_ptr<ProtoDatabase<ArticleEntry>::KeyEntryVector> entries_to_save(
      new ProtoDatabase<ArticleEntry>::KeyEntryVector());
  std::unique_ptr<std::vector<std::string>> keys_to_remove(
      new std::vector<std::string>());

  for (auto it = change_list.begin(); it != change_list.end(); ++it) {
    if (it->change_type() == SyncChange::ACTION_DELETE) {
      ArticleEntry entry = GetEntryFromChange(*it);
      keys_to_remove->push_back(entry.entry_id());
    } else {
      ArticleEntry entry = GetEntryFromChange(*it);
      entries_to_save->push_back(std::make_pair(entry.entry_id(), entry));
    }
  }
  database_->UpdateEntries(std::move(entries_to_save),
                           std::move(keys_to_remove),
                           base::BindOnce(&DomDistillerStore::OnDatabaseSave,
                                          weak_ptr_factory_.GetWeakPtr()));
  return true;
}

void DomDistillerStore::MergeDataWithModel(const SyncDataList& data,
                                           SyncChangeList* changes_applied,
                                           SyncChangeList* changes_missing) {
  // TODO(cjhopman): This naive merge algorithm could cause flip-flopping
  // between database/sync of multiple clients.
  DCHECK(changes_applied);
  DCHECK(changes_missing);

  SyncChangeList changes_to_apply;
  model_.CalculateChangesForMerge(data, &changes_to_apply, changes_missing);
  ApplyChangesToModel(changes_to_apply, changes_applied, changes_missing);
}

}  // namespace dom_distiller
