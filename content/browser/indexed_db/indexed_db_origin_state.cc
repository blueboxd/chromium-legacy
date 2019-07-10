// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/indexed_db_origin_state.h"

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/rand_util.h"
#include "base/stl_util.h"
#include "content/browser/indexed_db/indexed_db_backing_store.h"
#include "content/browser/indexed_db/indexed_db_class_factory.h"
#include "content/browser/indexed_db/indexed_db_connection.h"
#include "content/browser/indexed_db/indexed_db_database.h"
#include "content/browser/indexed_db/indexed_db_factory_impl.h"
#include "content/browser/indexed_db/indexed_db_leveldb_coding.h"
#include "content/browser/indexed_db/indexed_db_leveldb_operations.h"
#include "content/browser/indexed_db/indexed_db_pre_close_task_queue.h"
#include "content/browser/indexed_db/indexed_db_tombstone_sweeper.h"
#include "content/browser/indexed_db/indexed_db_transaction.h"
#include "third_party/blink/public/platform/modules/indexeddb/web_idb_database_exception.h"

namespace content {
namespace {
// Time after the last connection to a database is closed and when we destroy
// the backing store.
const int64_t kBackingStoreGracePeriodSeconds = 2;
// Total time we let pre-close tasks run.
const int64_t kRunningPreCloseTasksMaxRunPeriodSeconds = 60;
// The number of iterations for every 'round' of the tombstone sweeper.
const int kTombstoneSweeperRoundIterations = 1000;
// The maximum total iterations for the tombstone sweeper.
const int kTombstoneSweeperMaxIterations = 10 * 1000 * 1000;

constexpr const base::TimeDelta kMinEarliestOriginSweepFromNow =
    base::TimeDelta::FromDays(1);
static_assert(kMinEarliestOriginSweepFromNow <
                  IndexedDBOriginState::kMaxEarliestOriginSweepFromNow,
              "Min < Max");

constexpr const base::TimeDelta kMinEarliestGlobalSweepFromNow =
    base::TimeDelta::FromMinutes(10);
static_assert(kMinEarliestGlobalSweepFromNow <
                  IndexedDBOriginState::kMaxEarliestGlobalSweepFromNow,
              "Min < Max");

base::Time GenerateNextOriginSweepTime(base::Time now) {
  uint64_t range =
      IndexedDBOriginState::kMaxEarliestOriginSweepFromNow.InMilliseconds() -
      kMinEarliestOriginSweepFromNow.InMilliseconds();
  int64_t rand_millis = kMinEarliestOriginSweepFromNow.InMilliseconds() +
                        static_cast<int64_t>(base::RandGenerator(range));
  return now + base::TimeDelta::FromMilliseconds(rand_millis);
}

base::Time GenerateNextGlobalSweepTime(base::Time now) {
  uint64_t range =
      IndexedDBOriginState::kMaxEarliestGlobalSweepFromNow.InMilliseconds() -
      kMinEarliestGlobalSweepFromNow.InMilliseconds();
  int64_t rand_millis = kMinEarliestGlobalSweepFromNow.InMilliseconds() +
                        static_cast<int64_t>(base::RandGenerator(range));
  return now + base::TimeDelta::FromMilliseconds(rand_millis);
}

}  // namespace

constexpr const base::TimeDelta
    IndexedDBOriginState::kMaxEarliestGlobalSweepFromNow;
constexpr const base::TimeDelta
    IndexedDBOriginState::kMaxEarliestOriginSweepFromNow;

IndexedDBOriginState::IndexedDBOriginState(
    bool persist_for_incognito,
    base::Clock* clock,
    indexed_db::LevelDBFactory* leveldb_factory,
    base::Time* earliest_global_sweep_time,
    base::OnceClosure destruct_myself,
    std::unique_ptr<IndexedDBBackingStore> backing_store)
    : persist_for_incognito_(persist_for_incognito),
      clock_(clock),
      leveldb_factory_(leveldb_factory),
      earliest_global_sweep_time_(earliest_global_sweep_time),
      lock_manager_(kIndexedDBLockLevelCount),
      backing_store_(std::move(backing_store)),
      destruct_myself_(std::move(destruct_myself)) {
  DCHECK(clock_);
  DCHECK(earliest_global_sweep_time_);
  if (*earliest_global_sweep_time_ == base::Time())
    *earliest_global_sweep_time_ = GenerateNextGlobalSweepTime(clock_->Now());
}

IndexedDBOriginState::~IndexedDBOriginState() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void IndexedDBOriginState::AbortAllTransactions(bool compact) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Because finishing all transactions could cause a database to be destructed
  // (which would mutate the database_ map), save the keys beforehand and use
  // those.
  std::vector<base::string16> origins;
  origins.reserve(databases_.size());
  for (const auto& pair : databases_) {
    origins.push_back(pair.first);
  }

  for (const base::string16& origin : origins) {
    auto it = databases_.find(origin);
    if (it == databases_.end())
      continue;

    // Calling FinishAllTransactions can destruct the IndexedDBConnection &
    // modify the IndexedDBDatabase::connection() list. To prevent UAFs, start
    // by taking a WeakPtr of all connections, and then iterate that list.
    std::vector<base::WeakPtr<IndexedDBConnection>> weak_connections;
    weak_connections.reserve(it->second->connections().size());
    for (IndexedDBConnection* connection : it->second->connections())
      weak_connections.push_back(connection->GetWeakPtr());

    for (base::WeakPtr<IndexedDBConnection> connection : weak_connections) {
      if (connection) {
        connection->FinishAllTransactions(IndexedDBDatabaseError(
            blink::kWebIDBDatabaseExceptionUnknownError,
            "Aborting all transactions for the origin."));
      }
    }
  }
  if (compact)
    backing_store_->Compact();
}

void IndexedDBOriginState::ForceClose() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // To avoid re-entry, the |db_destruction_weak_factory_| is invalidated so
  // that of the deletions closures returned by CreateDatabaseDeleteClosure will
  // no-op. This allows force closing all of the databases without having the
  // map mutate. Afterwards the map is manually deleted.
  IndexedDBOriginStateHandle handle = CreateHandle();
  db_destruction_weak_factory_.InvalidateWeakPtrs();
  for (const auto& pair : databases_) {
    pair.second->ForceClose();
  }
  databases_.clear();
  if (has_blobs_outstanding_) {
    backing_store_->active_blob_registry()->ForceShutdown();
    has_blobs_outstanding_ = false;
  }
  skip_closing_sequence_ = true;
}

void IndexedDBOriginState::ReportOutstandingBlobs(bool blobs_outstanding) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  has_blobs_outstanding_ = blobs_outstanding;
  MaybeStartClosing();
}

void IndexedDBOriginState::StopPersistingForIncognito() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  persist_for_incognito_ = false;
  MaybeStartClosing();
}

IndexedDBDatabase* IndexedDBOriginState::AddDatabase(
    const base::string16& name,
    std::unique_ptr<IndexedDBDatabase> database) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!base::Contains(databases_, name));
  return databases_.emplace(name, std::move(database)).first->second.get();
}

base::OnceClosure IndexedDBOriginState::CreateDatabaseDeleteClosure(
    const base::string16& name) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return base::BindOnce(
      [](base::WeakPtr<IndexedDBOriginState> factory,
         const base::string16& name) {
        if (!factory)
          return;
        DCHECK_CALLED_ON_VALID_SEQUENCE(factory->sequence_checker_);
        size_t delete_size = factory->databases_.erase(name);
        DCHECK(delete_size) << "Database " << name << " did not exist.";
      },
      db_destruction_weak_factory_.GetWeakPtr(), name);
}

IndexedDBOriginStateHandle IndexedDBOriginState::CreateHandle() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ++open_handles_;
  if (closing_stage_ != ClosingState::kNotClosing) {
    closing_stage_ = ClosingState::kNotClosing;
    close_timer_.AbandonAndStop();
    if (pre_close_task_queue_) {
      pre_close_task_queue_->StopForNewConnection();
      pre_close_task_queue_.reset();
    }
  }
  return IndexedDBOriginStateHandle(weak_factory_.GetWeakPtr());
}

void IndexedDBOriginState::OnHandleDestruction() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_GT(open_handles_, 0ll);
  --open_handles_;
  MaybeStartClosing();
}

bool IndexedDBOriginState::CanCloseFactory() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_GE(open_handles_, 0);
  return !has_blobs_outstanding_ && open_handles_ <= 0 &&
         !persist_for_incognito_;
}

void IndexedDBOriginState::MaybeStartClosing() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!IsClosing() && CanCloseFactory())
    StartClosing();
}

void IndexedDBOriginState::StartClosing() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(CanCloseFactory());
  DCHECK(!IsClosing());

  if (skip_closing_sequence_ ||
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          kIDBCloseImmediatelySwitch)) {
    closing_stage_ = ClosingState::kClosed;
    CloseAndDestruct();
    return;
  }

  // Start a timer to close the backing store, unless something else opens it
  // in the mean time.
  DCHECK(!close_timer_.IsRunning());
  closing_stage_ = ClosingState::kPreCloseGracePeriod;
  close_timer_.Start(
      FROM_HERE, base::TimeDelta::FromSeconds(kBackingStoreGracePeriodSeconds),
      base::BindOnce(
          [](base::WeakPtr<IndexedDBOriginState> factory) {
            if (!factory ||
                factory->closing_stage_ != ClosingState::kPreCloseGracePeriod)
              return;
            factory->StartPreCloseTasks();
          },
          weak_factory_.GetWeakPtr()));
}

void IndexedDBOriginState::StartPreCloseTasks() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(closing_stage_ == ClosingState::kPreCloseGracePeriod);
  closing_stage_ = ClosingState::kRunningPreCloseTasks;

  // The callback will run on all early returns in this function.
  base::ScopedClosureRunner maybe_close_backing_store_runner(base::BindOnce(
      [](base::WeakPtr<IndexedDBOriginState> factory) {
        if (!factory ||
            factory->closing_stage_ != ClosingState::kRunningPreCloseTasks)
          return;
        factory->closing_stage_ = ClosingState::kClosed;
        factory->pre_close_task_queue_.reset();
        factory->CloseAndDestruct();
      },
      weak_factory_.GetWeakPtr()));

  base::Time now = clock_->Now();

  // Check that the last sweep hasn't run too recently.
  if (*earliest_global_sweep_time_ > now)
    return;

  base::Time origin_earliest_sweep;
  leveldb::Status s = indexed_db::GetEarliestSweepTime(backing_store_->db(),
                                                       &origin_earliest_sweep);
  // TODO(dmurph): Log this or report to UMA.
  if (!s.ok() && !s.IsNotFound())
    return;

  // This origin hasn't been swept too recently.
  if (origin_earliest_sweep > now)
    return;

  // A sweep will happen now, so reset the sweep timers.
  *earliest_global_sweep_time_ = GenerateNextGlobalSweepTime(now);
  scoped_refptr<TransactionalLevelDBTransaction> txn =
      leveldb_factory_->CreateLevelDBTransaction(backing_store_->db());
  indexed_db::SetEarliestSweepTime(txn.get(), GenerateNextOriginSweepTime(now));
  s = txn->Commit();

  // TODO(dmurph): Log this or report to UMA.
  if (!s.ok())
    return;

  std::list<std::unique_ptr<IndexedDBPreCloseTaskQueue::PreCloseTask>> tasks;
  tasks.push_back(std::make_unique<IndexedDBTombstoneSweeper>(
      kTombstoneSweeperRoundIterations, kTombstoneSweeperMaxIterations,
      backing_store_->db()->db()));
  // TODO(dmurph): Add compaction task that compacts all indexes if we have
  // more than X deletions.

  pre_close_task_queue_ = std::make_unique<IndexedDBPreCloseTaskQueue>(
      std::move(tasks), maybe_close_backing_store_runner.Release(),
      base::TimeDelta::FromSeconds(kRunningPreCloseTasksMaxRunPeriodSeconds),
      std::make_unique<base::OneShotTimer>());
  pre_close_task_queue_->Start(
      base::BindOnce(&IndexedDBBackingStore::GetCompleteMetadata,
                     base::Unretained(backing_store_.get())));
}

void IndexedDBOriginState::CloseAndDestruct() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(CanCloseFactory());
  DCHECK(closing_stage_ == ClosingState::kClosed);
  close_timer_.AbandonAndStop();
  pre_close_task_queue_.reset();

  if (backing_store_ && backing_store_->IsBlobCleanupPending())
    backing_store_->ForceRunBlobCleanup();

  std::move(destruct_myself_).Run();
}

}  // namespace content
