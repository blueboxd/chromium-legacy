// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/dom_distiller/core/dom_distiller_model.h"

#include <unordered_set>
#include <utility>

using syncer::SyncChange;
using syncer::SyncChangeList;
using syncer::SyncData;
using syncer::SyncDataList;

namespace dom_distiller {

DomDistillerModel::DomDistillerModel() : next_key_(1) {}

DomDistillerModel::DomDistillerModel(
    const std::vector<ArticleEntry>& initial_data)
    : next_key_(1) {
  for (size_t i = 0; i < initial_data.size(); ++i) {
    AddEntry(initial_data[i]);
  }
}

DomDistillerModel::~DomDistillerModel() {}

bool DomDistillerModel::GetEntryById(const std::string& entry_id,
                                     ArticleEntry* entry) const {
  KeyType key = 0;
  if (!GetKeyById(entry_id, &key)) {
    return false;
  }
  GetEntryByKey(key, entry);
  return true;
}

bool DomDistillerModel::GetEntryByUrl(const GURL& url,
                                      ArticleEntry* entry) const {
  KeyType key = 0;
  if (!GetKeyByUrl(url, &key)) {
    return false;
  }
  GetEntryByKey(key, entry);
  return true;
}

bool DomDistillerModel::GetKeyById(const std::string& entry_id,
                                   KeyType* key) const {
  auto it = entry_id_to_key_map_.find(entry_id);
  if (it == entry_id_to_key_map_.end()) {
    return false;
  }
  if (key != nullptr) {
    *key = it->second;
  }
  return true;
}

bool DomDistillerModel::GetKeyByUrl(const GURL& url, KeyType* key) const {
  auto it = url_to_key_map_.find(url.spec());
  if (it == url_to_key_map_.end()) {
    return false;
  }
  if (key != nullptr) {
    *key = it->second;
  }
  return true;
}

void DomDistillerModel::GetEntryByKey(KeyType key, ArticleEntry* entry) const {
  if (entry != nullptr) {
    auto it = entries_.find(key);
    DCHECK(it != entries_.end());
    *entry = it->second;
  }
}

size_t DomDistillerModel::GetNumEntries() const {
  return entries_.size();
}

std::vector<ArticleEntry> DomDistillerModel::GetEntries() const {
  std::vector<ArticleEntry> entries_list;
  for (auto it = entries_.begin(); it != entries_.end(); ++it) {
    entries_list.push_back(it->second);
  }
  return entries_list;
}

void DomDistillerModel::AddEntry(const ArticleEntry& entry) {
  const std::string& entry_id = entry.entry_id();
  KeyType key = next_key_++;
  DCHECK(!GetKeyById(entry_id, nullptr));
  entries_.insert(std::make_pair(key, entry));
  entry_id_to_key_map_.insert(std::make_pair(entry_id, key));
  for (int i = 0; i < entry.pages_size(); ++i) {
    url_to_key_map_.insert(std::make_pair(entry.pages(i).url(), key));
  }
}

}  // namespace dom_distiller
