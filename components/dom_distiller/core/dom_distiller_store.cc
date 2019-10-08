// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/dom_distiller/core/dom_distiller_store.h"

#include <stddef.h>

#include <utility>

namespace dom_distiller {

DomDistillerStore::DomDistillerStore() {}

DomDistillerStore::DomDistillerStore(
    const std::vector<ArticleEntry>& initial_data)
    : model_(initial_data) {}

DomDistillerStore::~DomDistillerStore() {}

bool DomDistillerStore::GetEntryById(const std::string& entry_id,
                                     ArticleEntry* entry) {
  return model_.GetEntryById(entry_id, entry);
}

bool DomDistillerStore::GetEntryByUrl(const GURL& url, ArticleEntry* entry) {
  return model_.GetEntryByUrl(url, entry);
}

std::vector<ArticleEntry> DomDistillerStore::GetEntries() const {
  return model_.GetEntries();
}

}  // namespace dom_distiller
