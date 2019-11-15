// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_SYNCABLE_SYNCABLE_DELETE_JOURNAL_H_
#define COMPONENTS_SYNC_SYNCABLE_SYNCABLE_DELETE_JOURNAL_H_

#include "base/macros.h"
#include "components/sync/syncable/metahandle_set.h"

namespace syncer {
namespace syncable {

class BaseTransaction;

// DeleteJournal manages deleted entries that are not in sync directory until
// it's safe to drop them after the deletion is confirmed with native models.
// DeleteJournal is thread-safe and can be accessed on any thread. Has to hold
// a valid transaction object when calling methods of DeleteJournal, thus each
// method requires a non-null |trans| parameter.
class DeleteJournal {
 public:
  DeleteJournal();
  ~DeleteJournal();

  // Purge entries of specified type in |delete_journals_| if their handles are
  // in |to_purge|. This should be called after model association and
  // |to_purge| should contain handles of the entries whose deletions are
  // confirmed in native model. Can be called on any thread.
  void PurgeDeleteJournals(BaseTransaction* trans,
                           const MetahandleSet& to_purge);

  // Move handles in |delete_journals_to_purge_| to |journals_to_purge|. Called
  // on sync thread.
  void TakeSnapshotAndClear(BaseTransaction* trans,
                            MetahandleSet* journals_to_purge);

 private:
  // Contains meta handles of deleted entries that have been persisted or
  // undeleted, thus can be removed from database.
  MetahandleSet delete_journals_to_purge_;

  DISALLOW_COPY_AND_ASSIGN(DeleteJournal);
};

}  // namespace syncable
}  // namespace syncer

#endif  // COMPONENTS_SYNC_SYNCABLE_SYNCABLE_DELETE_JOURNAL_H_
