// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/syncable/syncable_delete_journal.h"

#include <stdint.h>

#include <utility>

#include "components/sync/base/model_type.h"

namespace syncer {
namespace syncable {

DeleteJournal::DeleteJournal(std::unique_ptr<JournalIndex> initial_journal) {
  DCHECK(initial_journal);
  delete_journals_.swap(*initial_journal);
}

DeleteJournal::~DeleteJournal() {}

void DeleteJournal::PurgeDeleteJournals(BaseTransaction* trans,
                                        const MetahandleSet& to_purge) {
  DCHECK(trans);
  auto it = delete_journals_.begin();
  while (it != delete_journals_.end()) {
    int64_t handle = (*it).first->ref(META_HANDLE);
    if (to_purge.count(handle)) {
      delete_journals_.erase(it++);
    } else {
      ++it;
    }
  }
  delete_journals_to_purge_.insert(to_purge.begin(), to_purge.end());
}

void DeleteJournal::TakeSnapshotAndClear(BaseTransaction* trans,
                                         MetahandleSet* journals_to_purge) {
  DCHECK(trans);
  *journals_to_purge = delete_journals_to_purge_;
  delete_journals_to_purge_.clear();
}

// static
void DeleteJournal::AddEntryToJournalIndex(JournalIndex* journal_index,
                                           std::unique_ptr<EntryKernel> entry) {
  EntryKernel* key = entry.get();
  if (journal_index->find(key) == journal_index->end())
    (*journal_index)[key] = std::move(entry);
}

}  // namespace syncable
}  // namespace syncer
