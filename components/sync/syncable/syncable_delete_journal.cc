// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/syncable/syncable_delete_journal.h"

#include "base/logging.h"

namespace syncer {
namespace syncable {

DeleteJournal::DeleteJournal() {}

DeleteJournal::~DeleteJournal() {}

void DeleteJournal::PurgeDeleteJournals(BaseTransaction* trans,
                                        const MetahandleSet& to_purge) {
  DCHECK(trans);
  delete_journals_to_purge_.insert(to_purge.begin(), to_purge.end());
}

void DeleteJournal::TakeSnapshotAndClear(BaseTransaction* trans,
                                         MetahandleSet* journals_to_purge) {
  DCHECK(trans);
  *journals_to_purge = delete_journals_to_purge_;
  delete_journals_to_purge_.clear();
}

}  // namespace syncable
}  // namespace syncer
