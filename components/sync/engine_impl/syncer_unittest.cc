// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/engine_impl/syncer.h"

#include <stddef.h>

#include <algorithm>
#include <limits>
#include <list>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/compiler_specific.h"
#include "base/location.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/sync/base/cancelation_signal.h"
#include "components/sync/base/extensions_activity.h"
#include "components/sync/base/time.h"
#include "components/sync/engine/cycle/commit_counters.h"
#include "components/sync/engine/cycle/status_counters.h"
#include "components/sync/engine/cycle/update_counters.h"
#include "components/sync/engine/model_safe_worker.h"
#include "components/sync/engine_impl/backoff_delay_provider.h"
#include "components/sync/engine_impl/cycle/mock_debug_info_getter.h"
#include "components/sync/engine_impl/cycle/sync_cycle_context.h"
#include "components/sync/engine_impl/get_commit_ids.h"
#include "components/sync/engine_impl/net/server_connection_manager.h"
#include "components/sync/engine_impl/sync_scheduler_impl.h"
#include "components/sync/engine_impl/syncer_proto_util.h"
#include "components/sync/nigori/keystore_keys_handler.h"
#include "components/sync/protocol/bookmark_specifics.pb.h"
#include "components/sync/protocol/preference_specifics.pb.h"
#include "components/sync/syncable/mutable_entry.h"
#include "components/sync/syncable/syncable_read_transaction.h"
#include "components/sync/syncable/syncable_util.h"
#include "components/sync/syncable/syncable_write_transaction.h"
#include "components/sync/syncable/test_user_share.h"
#include "components/sync/test/engine/fake_model_worker.h"
#include "components/sync/test/engine/mock_connection_manager.h"
#include "components/sync/test/engine/mock_nudge_handler.h"
#include "components/sync/test/engine/test_id_factory.h"
#include "components/sync/test/engine/test_syncable_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::TimeDelta;

using std::count;
using std::map;
using std::multimap;
using std::set;
using std::string;
using std::vector;

namespace syncer {

using syncable::CountEntriesWithName;
using syncable::Directory;
using syncable::Entry;
using syncable::GetFirstEntryWithName;
using syncable::GetOnlyEntryWithName;
using syncable::Id;
using syncable::MutableEntry;

using syncable::CREATE;
using syncable::GET_BY_HANDLE;
using syncable::GET_BY_ID;
using syncable::GET_BY_CLIENT_TAG;
using syncable::GET_BY_SERVER_TAG;
using syncable::GET_TYPE_ROOT;
using syncable::UNITTEST;

namespace {

// A helper to hold on to the counters emitted by the sync engine.
class TypeDebugInfoCache : public TypeDebugInfoObserver {
 public:
  TypeDebugInfoCache();
  ~TypeDebugInfoCache() override;

  CommitCounters GetLatestCommitCounters(ModelType type) const;
  UpdateCounters GetLatestUpdateCounters(ModelType type) const;
  StatusCounters GetLatestStatusCounters(ModelType type) const;

  // TypeDebugInfoObserver implementation.
  void OnCommitCountersUpdated(ModelType type,
                               const CommitCounters& counters) override;
  void OnUpdateCountersUpdated(ModelType type,
                               const UpdateCounters& counters) override;
  void OnStatusCountersUpdated(ModelType type,
                               const StatusCounters& counters) override;

 private:
  std::map<ModelType, CommitCounters> commit_counters_map_;
  std::map<ModelType, UpdateCounters> update_counters_map_;
  std::map<ModelType, StatusCounters> status_counters_map_;
};

TypeDebugInfoCache::TypeDebugInfoCache() {}

TypeDebugInfoCache::~TypeDebugInfoCache() {}

CommitCounters TypeDebugInfoCache::GetLatestCommitCounters(
    ModelType type) const {
  auto it = commit_counters_map_.find(type);
  if (it == commit_counters_map_.end()) {
    return CommitCounters();
  } else {
    return it->second;
  }
}

UpdateCounters TypeDebugInfoCache::GetLatestUpdateCounters(
    ModelType type) const {
  auto it = update_counters_map_.find(type);
  if (it == update_counters_map_.end()) {
    return UpdateCounters();
  } else {
    return it->second;
  }
}

StatusCounters TypeDebugInfoCache::GetLatestStatusCounters(
    ModelType type) const {
  auto it = status_counters_map_.find(type);
  if (it == status_counters_map_.end()) {
    return StatusCounters();
  } else {
    return it->second;
  }
}

void TypeDebugInfoCache::OnCommitCountersUpdated(
    ModelType type,
    const CommitCounters& counters) {
  commit_counters_map_[type] = counters;
}

void TypeDebugInfoCache::OnUpdateCountersUpdated(
    ModelType type,
    const UpdateCounters& counters) {
  update_counters_map_[type] = counters;
}

void TypeDebugInfoCache::OnStatusCountersUpdated(
    ModelType type,
    const StatusCounters& counters) {
  status_counters_map_[type] = counters;
}

}  // namespace

// Syncer unit tests. Unfortunately a lot of these tests
// are outdated and need to be reworked and updated.
class SyncerTest : public testing::Test,
                   public SyncCycle::Delegate,
                   public SyncEngineEventListener {
 protected:
  SyncerTest()
      : extensions_activity_(new ExtensionsActivity),
        syncer_(nullptr),
        last_client_invalidation_hint_buffer_size_(10) {}

  // SyncCycle::Delegate implementation.
  void OnThrottled(const base::TimeDelta& throttle_duration) override {
    FAIL() << "Should not get silenced.";
  }
  void OnTypesThrottled(ModelTypeSet types,
                        const base::TimeDelta& throttle_duration) override {
    scheduler_->OnTypesThrottled(types, throttle_duration);
  }
  void OnTypesBackedOff(ModelTypeSet types) override {
    scheduler_->OnTypesBackedOff(types);
  }
  bool IsAnyThrottleOrBackoff() override { return false; }
  void OnReceivedPollIntervalUpdate(
      const base::TimeDelta& new_interval) override {
    last_poll_interval_received_ = new_interval;
  }
  void OnReceivedCustomNudgeDelays(
      const std::map<ModelType, base::TimeDelta>& delay_map) override {
    auto iter = delay_map.find(SESSIONS);
    if (iter != delay_map.end() && iter->second > base::TimeDelta())
      last_sessions_commit_delay_ = iter->second;
    iter = delay_map.find(BOOKMARKS);
    if (iter != delay_map.end() && iter->second > base::TimeDelta())
      last_bookmarks_commit_delay_ = iter->second;
  }
  void OnReceivedClientInvalidationHintBufferSize(int size) override {
    last_client_invalidation_hint_buffer_size_ = size;
  }
  void OnReceivedGuRetryDelay(const base::TimeDelta& delay) override {}
  void OnReceivedMigrationRequest(ModelTypeSet types) override {}
  void OnProtocolEvent(const ProtocolEvent& event) override {}
  void OnSyncProtocolError(const SyncProtocolError& error) override {}

  void OnSyncCycleEvent(const SyncCycleEvent& event) override {
    DVLOG(1) << "HandleSyncEngineEvent in unittest " << event.what_happened;
    // we only test for entry-specific events, not status changed ones.
    switch (event.what_happened) {
      case SyncCycleEvent::SYNC_CYCLE_BEGIN:  // Fall through.
      case SyncCycleEvent::STATUS_CHANGED:
      case SyncCycleEvent::SYNC_CYCLE_ENDED:
        return;
      default:
        FAIL() << "Handling unknown error type in unit tests!!";
    }
  }

  void OnActionableError(const SyncProtocolError& error) override {}
  void OnRetryTimeChanged(base::Time retry_time) override {}
  void OnThrottledTypesChanged(ModelTypeSet throttled_types) override {}
  void OnBackedOffTypesChanged(ModelTypeSet backed_off_types) override {}
  void OnMigrationRequested(ModelTypeSet types) override {}

  void ResetCycle() {
    cycle_ = std::make_unique<SyncCycle>(context_.get(), this);
  }

  bool SyncShareNudge() {
    ResetCycle();

    // Pretend we've seen a local change, to make the nudge_tracker look normal.
    nudge_tracker_.RecordLocalChange(ModelTypeSet(BOOKMARKS));

    return syncer_->NormalSyncShare(context_->GetEnabledTypes(),
                                    &nudge_tracker_, cycle_.get());
  }

  bool SyncShareConfigure() {
    return SyncShareConfigureTypes(context_->GetEnabledTypes());
  }

  bool SyncShareConfigureTypes(ModelTypeSet types) {
    ResetCycle();
    return syncer_->ConfigureSyncShare(
        types, sync_pb::SyncEnums::RECONFIGURATION, cycle_.get());
  }

  void SetUp() override {
    test_user_share_.SetUp();
    mock_server_ = std::make_unique<MockConnectionManager>(directory());
    debug_info_getter_ = std::make_unique<MockDebugInfoGetter>();
    workers_.push_back(
        scoped_refptr<ModelSafeWorker>(new FakeModelWorker(GROUP_PASSIVE)));
    std::vector<SyncEngineEventListener*> listeners;
    listeners.push_back(this);

    model_type_registry_ = std::make_unique<ModelTypeRegistry>(
        workers_, test_user_share_.user_share(), &mock_nudge_handler_,
        &cancelation_signal_, test_user_share_.keystore_keys_handler());
    model_type_registry_->RegisterDirectoryTypeDebugInfoObserver(
        &debug_info_cache_);

    EnableDatatype(BOOKMARKS);
    EnableDatatype(EXTENSIONS);
    EnableDatatype(NIGORI);
    EnableDatatype(PREFERENCES);

    context_ = std::make_unique<SyncCycleContext>(
        mock_server_.get(), directory(), extensions_activity_.get(), listeners,
        debug_info_getter_.get(), model_type_registry_.get(),
        "fake_invalidator_client_id", mock_server_->store_birthday(),
        "fake_bag_of_chips",
        /*poll_interval=*/base::TimeDelta::FromMinutes(30));
    syncer_ = new Syncer(&cancelation_signal_);
    scheduler_ = std::make_unique<SyncSchedulerImpl>(
        "TestSyncScheduler", BackoffDelayProvider::FromDefaults(),
        context_.get(),
        // scheduler_ owned syncer_ now and will manage the memory of syncer_
        syncer_, false);

    syncable::ReadTransaction trans(FROM_HERE, directory());
    Directory::Metahandles children;
    directory()->GetChildHandlesById(&trans, trans.root_id(), &children);
    ASSERT_EQ(0u, children.size());
    root_id_ = TestIdFactory::root();
    parent_id_ = ids_.MakeServer("parent id");
    child_id_ = ids_.MakeServer("child id");
    mock_server_->SetKeystoreKey("encryption_key");
  }

  void TearDown() override {
    model_type_registry_->UnregisterDirectoryTypeDebugInfoObserver(
        &debug_info_cache_);
    mock_server_.reset();
    scheduler_.reset();
    test_user_share_.TearDown();
  }

  void WriteTestDataToEntry(syncable::WriteTransaction* trans,
                            MutableEntry* entry) {
    EXPECT_FALSE(entry->GetIsDir());
    EXPECT_FALSE(entry->GetIsDel());
    sync_pb::EntitySpecifics specifics;
    specifics.mutable_bookmark()->set_url("http://demo/");
    specifics.mutable_bookmark()->set_favicon("PNG");
    entry->PutSpecifics(specifics);
    entry->PutIsUnsynced(true);
  }
  void VerifyTestDataInEntry(syncable::BaseTransaction* trans, Entry* entry) {
    EXPECT_FALSE(entry->GetIsDir());
    EXPECT_FALSE(entry->GetIsDel());
    VerifyTestBookmarkDataInEntry(entry);
  }
  void VerifyTestBookmarkDataInEntry(Entry* entry) {
    const sync_pb::EntitySpecifics& specifics = entry->GetSpecifics();
    EXPECT_TRUE(specifics.has_bookmark());
    EXPECT_EQ("PNG", specifics.bookmark().favicon());
    EXPECT_EQ("http://demo/", specifics.bookmark().url());
  }

  void VerifyHierarchyConflictsReported(
      const sync_pb::ClientToServerMessage& message) {
    // Our request should have included a warning about hierarchy conflicts.
    const sync_pb::ClientStatus& client_status = message.client_status();
    EXPECT_TRUE(client_status.has_hierarchy_conflict_detected());
    EXPECT_TRUE(client_status.hierarchy_conflict_detected());
  }

  void VerifyNoHierarchyConflictsReported(
      const sync_pb::ClientToServerMessage& message) {
    // Our request should have reported no hierarchy conflicts detected.
    const sync_pb::ClientStatus& client_status = message.client_status();
    EXPECT_TRUE(client_status.has_hierarchy_conflict_detected());
    EXPECT_FALSE(client_status.hierarchy_conflict_detected());
  }

  void VerifyHierarchyConflictsUnspecified(
      const sync_pb::ClientToServerMessage& message) {
    // Our request should have neither confirmed nor denied hierarchy conflicts.
    const sync_pb::ClientStatus& client_status = message.client_status();
    EXPECT_FALSE(client_status.has_hierarchy_conflict_detected());
  }

  sync_pb::EntitySpecifics DefaultBookmarkSpecifics() {
    sync_pb::EntitySpecifics result;
    AddDefaultFieldValue(BOOKMARKS, &result);
    return result;
  }

  sync_pb::EntitySpecifics DefaultPreferencesSpecifics() {
    sync_pb::EntitySpecifics result;
    AddDefaultFieldValue(PREFERENCES, &result);
    return result;
  }

  CommitCounters GetCommitCounters(ModelType type) {
    return debug_info_cache_.GetLatestCommitCounters(type);
  }

  UpdateCounters GetUpdateCounters(ModelType type) {
    return debug_info_cache_.GetLatestUpdateCounters(type);
  }

  StatusCounters GetStatusCounters(ModelType type) {
    return debug_info_cache_.GetLatestStatusCounters(type);
  }

  Directory* directory() {
    return test_user_share_.user_share()->directory.get();
  }

  const std::string local_cache_guid() { return directory()->cache_guid(); }

  const std::string foreign_cache_guid() { return "kqyg7097kro6GSUod+GSg=="; }

  int64_t CreateUnsyncedDirectory(const string& entry_name,
                                  const string& idstring) {
    return CreateUnsyncedDirectory(entry_name,
                                   syncable::Id::CreateFromServerId(idstring));
  }

  int64_t CreateUnsyncedDirectory(const string& entry_name,
                                  const syncable::Id& id) {
    syncable::WriteTransaction wtrans(FROM_HERE, UNITTEST, directory());
    MutableEntry entry(&wtrans, CREATE, BOOKMARKS, wtrans.root_id(),
                       entry_name);
    EXPECT_TRUE(entry.good());
    entry.PutIsUnsynced(true);
    entry.PutIsDir(true);
    entry.PutSpecifics(DefaultBookmarkSpecifics());
    entry.PutBaseVersion(id.ServerKnows() ? 1 : 0);
    entry.PutId(id);
    return entry.GetMetahandle();
  }

  void EnableDatatype(ModelType model_type) {
    enabled_datatypes_.Put(model_type);
    model_type_registry_->RegisterDirectoryType(model_type, GROUP_PASSIVE);
    mock_server_->ExpectGetUpdatesRequestTypes(enabled_datatypes_);
  }

  void DisableDatatype(ModelType model_type) {
    enabled_datatypes_.Remove(model_type);
    model_type_registry_->UnregisterDirectoryType(model_type);
    mock_server_->ExpectGetUpdatesRequestTypes(enabled_datatypes_);
  }

  // Configures SyncCycleContext and NudgeTracker so Syncer won't call
  // GetUpdates prior to Commit. This method can be used to ensure a Commit is
  // not preceeded by GetUpdates.
  void ConfigureNoGetUpdatesRequired() {
    nudge_tracker_.OnInvalidationsEnabled();
    nudge_tracker_.RecordSuccessfulSyncCycle(ProtocolTypes());

    ASSERT_FALSE(nudge_tracker_.IsGetUpdatesRequired(ProtocolTypes()));
  }

  base::test::SingleThreadTaskEnvironment task_environment_;

  // Some ids to aid tests. Only the root one's value is specific. The rest
  // are named for test clarity.
  // TODO(chron): Get rid of these inbuilt IDs. They only make it
  // more confusing.
  syncable::Id root_id_;
  syncable::Id parent_id_;
  syncable::Id child_id_;

  TestIdFactory ids_;

  TestUserShare test_user_share_;
  scoped_refptr<ExtensionsActivity> extensions_activity_;
  std::unique_ptr<MockConnectionManager> mock_server_;
  CancelationSignal cancelation_signal_;

  Syncer* syncer_;

  std::unique_ptr<SyncCycle> cycle_;
  TypeDebugInfoCache debug_info_cache_;
  MockNudgeHandler mock_nudge_handler_;
  std::unique_ptr<ModelTypeRegistry> model_type_registry_;
  std::unique_ptr<SyncSchedulerImpl> scheduler_;
  std::unique_ptr<SyncCycleContext> context_;
  base::TimeDelta last_poll_interval_received_;
  base::TimeDelta last_sessions_commit_delay_;
  base::TimeDelta last_bookmarks_commit_delay_;
  int last_client_invalidation_hint_buffer_size_;
  std::vector<scoped_refptr<ModelSafeWorker>> workers_;

  ModelTypeSet enabled_datatypes_;
  NudgeTracker nudge_tracker_;
  std::unique_ptr<MockDebugInfoGetter> debug_info_getter_;

 private:
  DISALLOW_COPY_AND_ASSIGN(SyncerTest);
};

TEST_F(SyncerTest, CommitFiltersThrottledEntries) {
  const ModelTypeSet throttled_types(BOOKMARKS);
  sync_pb::EntitySpecifics bookmark_data;
  AddDefaultFieldValue(BOOKMARKS, &bookmark_data);

  mock_server_->AddUpdateDirectory(1, 0, "A", 10, 10, foreign_cache_guid(),
                                   "-1");
  EXPECT_TRUE(SyncShareNudge());

  {
    syncable::WriteTransaction wtrans(FROM_HERE, UNITTEST, directory());
    MutableEntry A(&wtrans, GET_BY_ID, ids_.FromNumber(1));
    ASSERT_TRUE(A.good());
    A.PutIsUnsynced(true);
    A.PutSpecifics(bookmark_data);
    A.PutNonUniqueName("bookmark");
  }

  // Now sync without enabling bookmarks.
  mock_server_->ExpectGetUpdatesRequestTypes(
      Difference(context_->GetEnabledTypes(), throttled_types));
  ResetCycle();
  syncer_->NormalSyncShare(
      Difference(context_->GetEnabledTypes(), throttled_types), &nudge_tracker_,
      cycle_.get());

  {
    // Nothing should have been committed as bookmarks is throttled.
    syncable::ReadTransaction rtrans(FROM_HERE, directory());
    Entry entryA(&rtrans, GET_BY_ID, ids_.FromNumber(1));
    ASSERT_TRUE(entryA.good());
    EXPECT_TRUE(entryA.GetIsUnsynced());
  }

  // Sync again with bookmarks enabled.
  mock_server_->ExpectGetUpdatesRequestTypes(context_->GetEnabledTypes());
  EXPECT_TRUE(SyncShareNudge());
  {
    // It should have been committed.
    syncable::ReadTransaction rtrans(FROM_HERE, directory());
    Entry entryA(&rtrans, GET_BY_ID, ids_.FromNumber(1));
    ASSERT_TRUE(entryA.good());
    EXPECT_FALSE(entryA.GetIsUnsynced());
  }
}

// We use a macro so we can preserve the error location.
#define VERIFY_ENTRY(id, is_unapplied, is_unsynced, prev_initialized,         \
                     parent_id, version, server_version, id_fac, rtrans)      \
  do {                                                                        \
    Entry entryA(rtrans, GET_BY_ID, id_fac.FromNumber(id));                   \
    ASSERT_TRUE(entryA.good());                                               \
    /* We don't use EXPECT_EQ here because if the left side param is false,*/ \
    /* gcc 4.6 warns converting 'false' to pointer type for argument 1.*/     \
    EXPECT_TRUE(is_unsynced == entryA.GetIsUnsynced());                       \
    EXPECT_TRUE(is_unapplied == entryA.GetIsUnappliedUpdate());               \
    EXPECT_TRUE(prev_initialized == IsRealDataType(GetModelTypeFromSpecifics( \
                                        entryA.GetBaseServerSpecifics())));   \
    EXPECT_TRUE(parent_id == -1 ||                                            \
                entryA.GetParentId() == id_fac.FromNumber(parent_id));        \
    EXPECT_EQ(version, entryA.GetBaseVersion());                              \
    EXPECT_EQ(server_version, entryA.GetServerVersion());                     \
  } while (0)

TEST_F(SyncerTest, GetUpdatesPartialThrottled) {
  sync_pb::EntitySpecifics bookmark, pref;
  bookmark.mutable_bookmark()->set_legacy_canonicalized_title("title");
  pref.mutable_preference()->set_name("name");
  AddDefaultFieldValue(BOOKMARKS, &bookmark);
  AddDefaultFieldValue(PREFERENCES, &pref);

  // Normal sync, all the data types should get synced.
  mock_server_->AddUpdateSpecifics(1, 0, "A", 10, 10, true, 0, bookmark,
                                   foreign_cache_guid(), "-1");
  mock_server_->AddUpdateSpecifics(2, 1, "B", 10, 10, false, 2, bookmark,
                                   foreign_cache_guid(), "-2");
  mock_server_->AddUpdateSpecifics(3, 1, "C", 10, 10, false, 1, bookmark,
                                   foreign_cache_guid(), "-3");
  mock_server_->AddUpdateSpecifics(4, 0, "D", 10, 10, false, 0, pref);

  EXPECT_TRUE(SyncShareNudge());
  {
    // Initial state. Everything is normal.
    syncable::ReadTransaction rtrans(FROM_HERE, directory());
    VERIFY_ENTRY(1, false, false, false, 0, 10, 10, ids_, &rtrans);
    VERIFY_ENTRY(2, false, false, false, 1, 10, 10, ids_, &rtrans);
    VERIFY_ENTRY(3, false, false, false, 1, 10, 10, ids_, &rtrans);
    VERIFY_ENTRY(4, false, false, false, 0, 10, 10, ids_, &rtrans);
  }

  // Set BOOKMARKS throttled but PREFERENCES not,
  // then BOOKMARKS should not get synced but PREFERENCES should.
  ModelTypeSet throttled_types(BOOKMARKS);
  mock_server_->set_throttling(true);
  mock_server_->SetPartialFailureTypes(throttled_types);

  mock_server_->AddUpdateSpecifics(1, 0, "E", 20, 20, true, 0, bookmark,
                                   foreign_cache_guid(), "-1");
  mock_server_->AddUpdateSpecifics(2, 1, "F", 20, 20, false, 2, bookmark,
                                   foreign_cache_guid(), "-2");
  mock_server_->AddUpdateSpecifics(3, 1, "G", 20, 20, false, 1, bookmark,
                                   foreign_cache_guid(), "-3");
  mock_server_->AddUpdateSpecifics(4, 0, "H", 20, 20, false, 0, pref);
  {
    syncable::WriteTransaction wtrans(FROM_HERE, UNITTEST, directory());
    MutableEntry A(&wtrans, GET_BY_ID, ids_.FromNumber(1));
    MutableEntry B(&wtrans, GET_BY_ID, ids_.FromNumber(2));
    MutableEntry C(&wtrans, GET_BY_ID, ids_.FromNumber(3));
    MutableEntry D(&wtrans, GET_BY_ID, ids_.FromNumber(4));
    A.PutIsUnsynced(true);
    B.PutIsUnsynced(true);
    C.PutIsUnsynced(true);
    D.PutIsUnsynced(true);
  }
  EXPECT_TRUE(SyncShareNudge());
  {
    // BOOKMARKS throttled.
    syncable::ReadTransaction rtrans(FROM_HERE, directory());
    VERIFY_ENTRY(1, false, true, false, 0, 10, 10, ids_, &rtrans);
    VERIFY_ENTRY(2, false, true, false, 1, 10, 10, ids_, &rtrans);
    VERIFY_ENTRY(3, false, true, false, 1, 10, 10, ids_, &rtrans);
    VERIFY_ENTRY(4, false, false, false, 0, 21, 21, ids_, &rtrans);
  }

  // Unthrottled BOOKMARKS, then BOOKMARKS should get synced now.
  mock_server_->set_throttling(false);

  mock_server_->AddUpdateSpecifics(1, 0, "E", 30, 30, true, 0, bookmark,
                                   foreign_cache_guid(), "-1");
  mock_server_->AddUpdateSpecifics(2, 1, "F", 30, 30, false, 2, bookmark,
                                   foreign_cache_guid(), "-2");
  mock_server_->AddUpdateSpecifics(3, 1, "G", 30, 30, false, 1, bookmark,
                                   foreign_cache_guid(), "-3");
  mock_server_->AddUpdateSpecifics(4, 0, "H", 30, 30, false, 0, pref);
  EXPECT_TRUE(SyncShareNudge());
  {
    // BOOKMARKS unthrottled.
    syncable::ReadTransaction rtrans(FROM_HERE, directory());
    VERIFY_ENTRY(1, false, false, false, 0, 31, 31, ids_, &rtrans);
    VERIFY_ENTRY(2, false, false, false, 1, 31, 31, ids_, &rtrans);
    VERIFY_ENTRY(3, false, false, false, 1, 31, 31, ids_, &rtrans);
    VERIFY_ENTRY(4, false, false, false, 0, 30, 30, ids_, &rtrans);
  }
}

TEST_F(SyncerTest, GetUpdatesPartialFailure) {
  sync_pb::EntitySpecifics bookmark, pref;
  bookmark.mutable_bookmark()->set_legacy_canonicalized_title("title");
  pref.mutable_preference()->set_name("name");
  AddDefaultFieldValue(BOOKMARKS, &bookmark);
  AddDefaultFieldValue(PREFERENCES, &pref);

  // Normal sync, all the data types should get synced.
  mock_server_->AddUpdateSpecifics(1, 0, "A", 10, 10, true, 0, bookmark,
                                   foreign_cache_guid(), "-1");
  mock_server_->AddUpdateSpecifics(2, 1, "B", 10, 10, false, 2, bookmark,
                                   foreign_cache_guid(), "-2");
  mock_server_->AddUpdateSpecifics(3, 1, "C", 10, 10, false, 1, bookmark,
                                   foreign_cache_guid(), "-3");
  mock_server_->AddUpdateSpecifics(4, 0, "D", 10, 10, false, 0, pref);

  EXPECT_TRUE(SyncShareNudge());
  {
    // Initial state. Everything is normal.
    syncable::ReadTransaction rtrans(FROM_HERE, directory());
    VERIFY_ENTRY(1, false, false, false, 0, 10, 10, ids_, &rtrans);
    VERIFY_ENTRY(2, false, false, false, 1, 10, 10, ids_, &rtrans);
    VERIFY_ENTRY(3, false, false, false, 1, 10, 10, ids_, &rtrans);
    VERIFY_ENTRY(4, false, false, false, 0, 10, 10, ids_, &rtrans);
  }

  // Set BOOKMARKS failure but PREFERENCES not,
  // then BOOKMARKS should not get synced but PREFERENCES should.
  ModelTypeSet failed_types(BOOKMARKS);
  mock_server_->set_partial_failure(true);
  mock_server_->SetPartialFailureTypes(failed_types);

  mock_server_->AddUpdateSpecifics(1, 0, "E", 20, 20, true, 0, bookmark,
                                   foreign_cache_guid(), "-1");
  mock_server_->AddUpdateSpecifics(2, 1, "F", 20, 20, false, 2, bookmark,
                                   foreign_cache_guid(), "-2");
  mock_server_->AddUpdateSpecifics(3, 1, "G", 20, 20, false, 1, bookmark,
                                   foreign_cache_guid(), "-3");
  mock_server_->AddUpdateSpecifics(4, 0, "H", 20, 20, false, 0, pref);
  {
    syncable::WriteTransaction wtrans(FROM_HERE, UNITTEST, directory());
    MutableEntry A(&wtrans, GET_BY_ID, ids_.FromNumber(1));
    MutableEntry B(&wtrans, GET_BY_ID, ids_.FromNumber(2));
    MutableEntry C(&wtrans, GET_BY_ID, ids_.FromNumber(3));
    MutableEntry D(&wtrans, GET_BY_ID, ids_.FromNumber(4));
    A.PutIsUnsynced(true);
    B.PutIsUnsynced(true);
    C.PutIsUnsynced(true);
    D.PutIsUnsynced(true);
  }
  EXPECT_TRUE(SyncShareNudge());
  {
    // BOOKMARKS failed.
    syncable::ReadTransaction rtrans(FROM_HERE, directory());
    VERIFY_ENTRY(1, false, true, false, 0, 10, 10, ids_, &rtrans);
    VERIFY_ENTRY(2, false, true, false, 1, 10, 10, ids_, &rtrans);
    VERIFY_ENTRY(3, false, true, false, 1, 10, 10, ids_, &rtrans);
    VERIFY_ENTRY(4, false, false, false, 0, 21, 21, ids_, &rtrans);
  }

  // Set BOOKMARKS not partial failed, then BOOKMARKS should get synced now.
  mock_server_->set_partial_failure(false);

  mock_server_->AddUpdateSpecifics(1, 0, "E", 30, 30, true, 0, bookmark,
                                   foreign_cache_guid(), "-1");
  mock_server_->AddUpdateSpecifics(2, 1, "F", 30, 30, false, 2, bookmark,
                                   foreign_cache_guid(), "-2");
  mock_server_->AddUpdateSpecifics(3, 1, "G", 30, 30, false, 1, bookmark,
                                   foreign_cache_guid(), "-3");
  mock_server_->AddUpdateSpecifics(4, 0, "H", 30, 30, false, 0, pref);
  EXPECT_TRUE(SyncShareNudge());
  {
    // BOOKMARKS not failed.
    syncable::ReadTransaction rtrans(FROM_HERE, directory());
    VERIFY_ENTRY(1, false, false, false, 0, 31, 31, ids_, &rtrans);
    VERIFY_ENTRY(2, false, false, false, 1, 31, 31, ids_, &rtrans);
    VERIFY_ENTRY(3, false, false, false, 1, 31, 31, ids_, &rtrans);
    VERIFY_ENTRY(4, false, false, false, 0, 30, 30, ids_, &rtrans);
  }
}

#undef VERIFY_ENTRY

TEST_F(SyncerTest, TestGetUnsyncedAndSimpleCommit) {
  {
    syncable::WriteTransaction wtrans(FROM_HERE, UNITTEST, directory());
    MutableEntry parent(&wtrans, CREATE, BOOKMARKS, wtrans.root_id(), "Pete");
    ASSERT_TRUE(parent.good());
    parent.PutIsUnsynced(true);
    parent.PutIsDir(true);
    parent.PutSpecifics(DefaultBookmarkSpecifics());
    parent.PutBaseVersion(1);
    parent.PutId(parent_id_);
    MutableEntry child(&wtrans, CREATE, BOOKMARKS, parent_id_, "Pete");
    ASSERT_TRUE(child.good());
    child.PutId(child_id_);
    child.PutBaseVersion(1);
    WriteTestDataToEntry(&wtrans, &child);
  }

  EXPECT_TRUE(SyncShareNudge());
  ASSERT_EQ(2u, mock_server_->committed_ids().size());
  // If this test starts failing, be aware other sort orders could be valid.
  EXPECT_EQ(parent_id_, mock_server_->committed_ids()[0]);
  EXPECT_EQ(child_id_, mock_server_->committed_ids()[1]);
  {
    syncable::ReadTransaction rt(FROM_HERE, directory());
    Entry entry(&rt, GET_BY_ID, child_id_);
    ASSERT_TRUE(entry.good());
    VerifyTestDataInEntry(&rt, &entry);
  }
}

TEST_F(SyncerTest, TestBasicUpdate) {
  string id = "some_id";
  string parent_id = "0";
  string name = "in_root";
  int64_t version = 10;
  int64_t timestamp = 10;
  mock_server_->AddUpdateDirectory(id, parent_id, name, version, timestamp,
                                   foreign_cache_guid(), "-1");

  EXPECT_TRUE(SyncShareNudge());
  {
    syncable::WriteTransaction trans(FROM_HERE, UNITTEST, directory());
    Entry entry(&trans, GET_BY_ID, syncable::Id::CreateFromServerId("some_id"));
    ASSERT_TRUE(entry.good());
    EXPECT_TRUE(entry.GetIsDir());
    EXPECT_EQ(version, entry.GetServerVersion());
    EXPECT_EQ(version, entry.GetBaseVersion());
    EXPECT_FALSE(entry.GetIsUnappliedUpdate());
    EXPECT_FALSE(entry.GetIsUnsynced());
    EXPECT_FALSE(entry.GetServerIsDel());
    EXPECT_FALSE(entry.GetIsDel());
  }
}

TEST_F(SyncerTest, CommittingNewDeleted) {
  {
    syncable::WriteTransaction trans(FROM_HERE, UNITTEST, directory());
    MutableEntry entry(&trans, CREATE, BOOKMARKS, trans.root_id(), "bob");
    entry.PutIsUnsynced(true);
    entry.PutIsDel(true);
  }
  EXPECT_TRUE(SyncShareNudge());
  EXPECT_EQ(0u, mock_server_->committed_ids().size());
}

// Committing more than kDefaultMaxCommitBatchSize items requires that
// we post more than one commit command to the server.  This test makes
// sure that scenario works as expected.
TEST_F(SyncerTest, CommitManyItemsInOneGo_Success) {
  uint32_t num_batches = 3;
  uint32_t items_to_commit = kDefaultMaxCommitBatchSize * num_batches;
  {
    syncable::WriteTransaction trans(FROM_HERE, UNITTEST, directory());
    for (uint32_t i = 0; i < items_to_commit; i++) {
      string nameutf8 = base::NumberToString(i);
      string name(nameutf8.begin(), nameutf8.end());
      MutableEntry e(&trans, CREATE, BOOKMARKS, trans.root_id(), name);
      e.PutIsUnsynced(true);
      e.PutIsDir(true);
      e.PutSpecifics(DefaultBookmarkSpecifics());
    }
  }
  ASSERT_EQ(items_to_commit, directory()->unsynced_entity_count());

  EXPECT_TRUE(SyncShareNudge());
  EXPECT_EQ(num_batches, mock_server_->commit_messages().size());
  EXPECT_EQ(0, directory()->unsynced_entity_count());
}

// Test that a single failure to contact the server will cause us to exit the
// commit loop immediately.
TEST_F(SyncerTest, CommitManyItemsInOneGo_PostBufferFail) {
  uint32_t num_batches = 3;
  uint32_t items_to_commit = kDefaultMaxCommitBatchSize * num_batches;
  {
    syncable::WriteTransaction trans(FROM_HERE, UNITTEST, directory());
    for (uint32_t i = 0; i < items_to_commit; i++) {
      string nameutf8 = base::NumberToString(i);
      string name(nameutf8.begin(), nameutf8.end());
      MutableEntry e(&trans, CREATE, BOOKMARKS, trans.root_id(), name);
      e.PutIsUnsynced(true);
      e.PutIsDir(true);
      e.PutSpecifics(DefaultBookmarkSpecifics());
    }
  }
  ASSERT_EQ(items_to_commit, directory()->unsynced_entity_count());

  // The second commit should fail.  It will be preceded by one successful
  // GetUpdate and one succesful commit.
  mock_server_->FailNthPostBufferToPathCall(3);
  base::HistogramTester histogram_tester;
  EXPECT_FALSE(SyncShareNudge());

  EXPECT_EQ(1U, mock_server_->commit_messages().size());
  EXPECT_EQ(
      SyncerError::SYNC_SERVER_ERROR,
      cycle_->status_controller().model_neutral_state().commit_result.value());
  EXPECT_EQ(items_to_commit - kDefaultMaxCommitBatchSize,
            directory()->unsynced_entity_count());
  histogram_tester.ExpectBucketCount("Sync.CommitResponse.BOOKMARK",
                                     SyncerError::SYNC_SERVER_ERROR,
                                     /*count=*/1);
  histogram_tester.ExpectBucketCount("Sync.CommitResponse",
                                     SyncerError::SYNC_SERVER_ERROR,
                                     /*count=*/1);
}

// Test that a single conflict response from the server will cause us to exit
// the commit loop immediately.
TEST_F(SyncerTest, CommitManyItemsInOneGo_CommitConflict) {
  uint32_t num_batches = 2;
  uint32_t items_to_commit = kDefaultMaxCommitBatchSize * num_batches;
  {
    syncable::WriteTransaction trans(FROM_HERE, UNITTEST, directory());
    for (uint32_t i = 0; i < items_to_commit; i++) {
      string nameutf8 = base::NumberToString(i);
      string name(nameutf8.begin(), nameutf8.end());
      MutableEntry e(&trans, CREATE, BOOKMARKS, trans.root_id(), name);
      e.PutIsUnsynced(true);
      e.PutIsDir(true);
      e.PutSpecifics(DefaultBookmarkSpecifics());
    }
  }
  ASSERT_EQ(items_to_commit, directory()->unsynced_entity_count());

  // Return a CONFLICT response for the first item.
  mock_server_->set_conflict_n_commits(1);
  EXPECT_FALSE(SyncShareNudge());

  // We should stop looping at the first sign of trouble.
  EXPECT_EQ(1U, mock_server_->commit_messages().size());
  EXPECT_EQ(items_to_commit - (kDefaultMaxCommitBatchSize - 1),
            directory()->unsynced_entity_count());
}

// Tests that sending debug info events works.
TEST_F(SyncerTest, SendDebugInfoEventsOnGetUpdates_HappyCase) {
  debug_info_getter_->AddDebugEvent();
  debug_info_getter_->AddDebugEvent();

  EXPECT_TRUE(SyncShareNudge());

  // Verify we received one GetUpdates request with two debug info events.
  EXPECT_EQ(1U, mock_server_->requests().size());
  ASSERT_TRUE(mock_server_->last_request().has_get_updates());
  EXPECT_EQ(2, mock_server_->last_request().debug_info().events_size());

  EXPECT_TRUE(SyncShareNudge());

  // See that we received another GetUpdates request, but that it contains no
  // debug info events.
  EXPECT_EQ(2U, mock_server_->requests().size());
  ASSERT_TRUE(mock_server_->last_request().has_get_updates());
  EXPECT_EQ(0, mock_server_->last_request().debug_info().events_size());

  debug_info_getter_->AddDebugEvent();

  EXPECT_TRUE(SyncShareNudge());

  // See that we received another GetUpdates request and it contains one debug
  // info event.
  EXPECT_EQ(3U, mock_server_->requests().size());
  ASSERT_TRUE(mock_server_->last_request().has_get_updates());
  EXPECT_EQ(1, mock_server_->last_request().debug_info().events_size());
}

// Tests that debug info events are dropped on server error.
TEST_F(SyncerTest, SendDebugInfoEventsOnGetUpdates_PostFailsDontDrop) {
  debug_info_getter_->AddDebugEvent();
  debug_info_getter_->AddDebugEvent();

  mock_server_->FailNextPostBufferToPathCall();
  EXPECT_FALSE(SyncShareNudge());

  // Verify we attempted to send one GetUpdates request with two debug info
  // events.
  EXPECT_EQ(1U, mock_server_->requests().size());
  ASSERT_TRUE(mock_server_->last_request().has_get_updates());
  EXPECT_EQ(2, mock_server_->last_request().debug_info().events_size());

  EXPECT_TRUE(SyncShareNudge());

  // See that the client resent the two debug info events.
  EXPECT_EQ(2U, mock_server_->requests().size());
  ASSERT_TRUE(mock_server_->last_request().has_get_updates());
  EXPECT_EQ(2, mock_server_->last_request().debug_info().events_size());

  // The previous send was successful so this next one shouldn't generate any
  // debug info events.
  EXPECT_TRUE(SyncShareNudge());
  EXPECT_EQ(3U, mock_server_->requests().size());
  ASSERT_TRUE(mock_server_->last_request().has_get_updates());
  EXPECT_EQ(0, mock_server_->last_request().debug_info().events_size());
}

// Tests that commit failure with conflict will trigger GetUpdates for next
// cycle of sync
TEST_F(SyncerTest, CommitFailureWithConflict) {
  ConfigureNoGetUpdatesRequired();
  CreateUnsyncedDirectory("X", "id_X");
  EXPECT_FALSE(nudge_tracker_.IsGetUpdatesRequired(ProtocolTypes()));

  EXPECT_TRUE(SyncShareNudge());
  EXPECT_FALSE(nudge_tracker_.IsGetUpdatesRequired(ProtocolTypes()));

  CreateUnsyncedDirectory("Y", "id_Y");
  mock_server_->set_conflict_n_commits(1);
  EXPECT_FALSE(SyncShareNudge());
  EXPECT_TRUE(nudge_tracker_.IsGetUpdatesRequired(ProtocolTypes()));

  nudge_tracker_.RecordSuccessfulSyncCycle(ProtocolTypes());
  EXPECT_FALSE(nudge_tracker_.IsGetUpdatesRequired(ProtocolTypes()));
}

// Tests that sending debug info events on Commit works.
TEST_F(SyncerTest, SendDebugInfoEventsOnCommit_HappyCase) {
  // Make sure GetUpdate isn't call as it would "steal" debug info events before
  // Commit has a chance to send them.
  ConfigureNoGetUpdatesRequired();

  // Generate a debug info event and trigger a commit.
  debug_info_getter_->AddDebugEvent();
  CreateUnsyncedDirectory("X", "id_X");
  EXPECT_TRUE(SyncShareNudge());

  // Verify that the last request received is a Commit and that it contains a
  // debug info event.
  EXPECT_EQ(1U, mock_server_->requests().size());
  ASSERT_TRUE(mock_server_->last_request().has_commit());
  EXPECT_EQ(1, mock_server_->last_request().debug_info().events_size());

  // Generate another commit, but no debug info event.
  CreateUnsyncedDirectory("Y", "id_Y");
  EXPECT_TRUE(SyncShareNudge());

  // See that it was received and contains no debug info events.
  EXPECT_EQ(2U, mock_server_->requests().size());
  ASSERT_TRUE(mock_server_->last_request().has_commit());
  EXPECT_EQ(0, mock_server_->last_request().debug_info().events_size());
}

// Tests that debug info events are not dropped on server error.
TEST_F(SyncerTest, SendDebugInfoEventsOnCommit_PostFailsDontDrop) {
  // Make sure GetUpdate isn't call as it would "steal" debug info events before
  // Commit has a chance to send them.
  ConfigureNoGetUpdatesRequired();

  mock_server_->FailNextPostBufferToPathCall();

  // Generate a debug info event and trigger a commit.
  debug_info_getter_->AddDebugEvent();
  CreateUnsyncedDirectory("X", "id_X");
  EXPECT_FALSE(SyncShareNudge());

  // Verify that the last request sent is a Commit and that it contains a debug
  // info event.
  EXPECT_EQ(1U, mock_server_->requests().size());
  ASSERT_TRUE(mock_server_->last_request().has_commit());
  EXPECT_EQ(1, mock_server_->last_request().debug_info().events_size());

  // Try again.
  EXPECT_TRUE(SyncShareNudge());

  // Verify that we've received another Commit and that it contains a debug info
  // event (just like the previous one).
  EXPECT_EQ(2U, mock_server_->requests().size());
  ASSERT_TRUE(mock_server_->last_request().has_commit());
  EXPECT_EQ(1, mock_server_->last_request().debug_info().events_size());

  // Generate another commit and try again.
  CreateUnsyncedDirectory("Y", "id_Y");
  EXPECT_TRUE(SyncShareNudge());

  // See that it was received and contains no debug info events.
  EXPECT_EQ(3U, mock_server_->requests().size());
  ASSERT_TRUE(mock_server_->last_request().has_commit());
  EXPECT_EQ(0, mock_server_->last_request().debug_info().events_size());
}

TEST_F(SyncerTest, HugeConflict) {
  int item_count = 300;  // We should be able to do 300 or 3000 w/o issue.

  syncable::Id parent_id = ids_.NewServerId();
  syncable::Id last_id = parent_id;
  vector<syncable::Id> tree_ids;

  // Create a lot of updates for which the parent does not exist yet.
  // Generate a huge deep tree which should all fail to apply at first.
  {
    syncable::WriteTransaction trans(FROM_HERE, UNITTEST, directory());
    for (int i = 0; i < item_count; i++) {
      syncable::Id next_id = ids_.NewServerId();
      syncable::Id local_id = ids_.NewLocalId();
      tree_ids.push_back(next_id);
      mock_server_->AddUpdateDirectory(next_id, last_id, "BOB", 2, 20,
                                       foreign_cache_guid(),
                                       local_id.GetServerId());
      last_id = next_id;
    }
  }
  EXPECT_TRUE(SyncShareNudge());

  // Check they're in the expected conflict state.
  {
    syncable::ReadTransaction trans(FROM_HERE, directory());
    for (int i = 0; i < item_count; i++) {
      Entry e(&trans, GET_BY_ID, tree_ids[i]);
      // They should all exist but none should be applied.
      ASSERT_TRUE(e.good());
      EXPECT_TRUE(e.GetIsDel());
      EXPECT_TRUE(e.GetIsUnappliedUpdate());
    }
  }

  // Add the missing parent directory.
  mock_server_->AddUpdateDirectory(parent_id, TestIdFactory::root(), "BOB", 2,
                                   20, foreign_cache_guid(), "-3500");
  EXPECT_TRUE(SyncShareNudge());

  // Now they should all be OK.
  {
    syncable::ReadTransaction trans(FROM_HERE, directory());
    for (int i = 0; i < item_count; i++) {
      Entry e(&trans, GET_BY_ID, tree_ids[i]);
      ASSERT_TRUE(e.good());
      EXPECT_FALSE(e.GetIsDel());
      EXPECT_FALSE(e.GetIsUnappliedUpdate());
    }
  }
}

TEST_F(SyncerTest, DeletedEntryWithBadParentInLoopCalculation) {
  mock_server_->AddUpdateDirectory(1, 0, "bob", 1, 10, foreign_cache_guid(),
                                   "-1");
  EXPECT_TRUE(SyncShareNudge());
  {
    syncable::WriteTransaction trans(FROM_HERE, UNITTEST, directory());
    MutableEntry bob(&trans, GET_BY_ID, ids_.FromNumber(1));
    ASSERT_TRUE(bob.good());
    // This is valid, because the parent could have gone away a long time ago.
    bob.PutParentId(ids_.FromNumber(54));
    bob.PutIsDel(true);
    bob.PutIsUnsynced(true);
  }
  mock_server_->AddUpdateDirectory(2, 1, "fred", 1, 10, foreign_cache_guid(),
                                   "-2");
  EXPECT_TRUE(SyncShareNudge());
  EXPECT_TRUE(SyncShareNudge());
}

// See what happens if the IS_DIR bit gets flipped.  This can cause us
// all kinds of disasters.
TEST_F(SyncerTest, UpdateFlipsTheFolderBit) {
  // Local object: a deleted directory (container), revision 1, unsynced.
  {
    syncable::WriteTransaction trans(FROM_HERE, UNITTEST, directory());

    MutableEntry local_deleted(&trans, CREATE, BOOKMARKS, trans.root_id(),
                               "name");
    local_deleted.PutId(ids_.FromNumber(1));
    local_deleted.PutBaseVersion(1);
    local_deleted.PutIsDel(true);
    local_deleted.PutIsDir(true);
    local_deleted.PutIsUnsynced(true);
    local_deleted.PutSpecifics(DefaultBookmarkSpecifics());
  }

  // Server update: entry-type object (not a container), revision 10.
  mock_server_->AddUpdateBookmark(ids_.FromNumber(1), root_id_, "name", 10, 10,
                                  local_cache_guid(),
                                  ids_.FromNumber(1).GetServerId());

  // Don't attempt to commit.
  mock_server_->set_conflict_all_commits(true);

  // The syncer should not attempt to apply the invalid update.
  EXPECT_FALSE(SyncShareNudge());

  {
    syncable::ReadTransaction trans(FROM_HERE, directory());
    Entry local_deleted(&trans, GET_BY_ID, ids_.FromNumber(1));
    EXPECT_EQ(1, local_deleted.GetBaseVersion());
    EXPECT_FALSE(local_deleted.GetIsUnappliedUpdate());
    EXPECT_TRUE(local_deleted.GetIsUnsynced());
    EXPECT_TRUE(local_deleted.GetIsDel());
    EXPECT_TRUE(local_deleted.GetIsDir());
  }
}

TEST_F(SyncerTest, DontMergeTwoExistingItems) {
  mock_server_->set_conflict_all_commits(true);
  mock_server_->AddUpdateBookmark(1, 0, "base", 10, 10, foreign_cache_guid(),
                                  "-1");
  mock_server_->AddUpdateBookmark(2, 0, "base2", 10, 10, foreign_cache_guid(),
                                  "-2");
  EXPECT_TRUE(SyncShareNudge());
  {
    syncable::WriteTransaction trans(FROM_HERE, UNITTEST, directory());
    MutableEntry entry(&trans, GET_BY_ID, ids_.FromNumber(2));
    ASSERT_TRUE(entry.good());
    entry.PutNonUniqueName("Copy of base");
    entry.PutIsUnsynced(true);
  }
  mock_server_->AddUpdateBookmark(1, 0, "Copy of base", 50, 50,
                                  foreign_cache_guid(), "-1");
  EXPECT_FALSE(SyncShareNudge());
  {
    syncable::ReadTransaction trans(FROM_HERE, directory());
    Entry entry1(&trans, GET_BY_ID, ids_.FromNumber(1));
    EXPECT_FALSE(entry1.GetIsUnappliedUpdate());
    EXPECT_FALSE(entry1.GetIsUnsynced());
    EXPECT_FALSE(entry1.GetIsDel());
    Entry entry2(&trans, GET_BY_ID, ids_.FromNumber(2));
    EXPECT_FALSE(entry2.GetIsUnappliedUpdate());
    EXPECT_TRUE(entry2.GetIsUnsynced());
    EXPECT_FALSE(entry2.GetIsDel());
    EXPECT_EQ(entry1.GetNonUniqueName(), entry2.GetNonUniqueName());
  }
}

TEST_F(SyncerTest, TestUndeleteUpdate) {
  mock_server_->set_conflict_all_commits(true);
  mock_server_->AddUpdateDirectory(1, 0, "foo", 1, 1, foreign_cache_guid(),
                                   "-1");
  mock_server_->AddUpdateDirectory(2, 1, "bar", 1, 2, foreign_cache_guid(),
                                   "-2");
  EXPECT_TRUE(SyncShareNudge());
  mock_server_->AddUpdateDirectory(2, 1, "bar", 2, 3, foreign_cache_guid(),
                                   "-2");
  mock_server_->SetLastUpdateDeleted();
  EXPECT_TRUE(SyncShareNudge());

  int64_t metahandle;
  {
    syncable::ReadTransaction trans(FROM_HERE, directory());
    Entry entry(&trans, GET_BY_ID, ids_.FromNumber(2));
    ASSERT_TRUE(entry.good());
    EXPECT_TRUE(entry.GetIsDel());
    metahandle = entry.GetMetahandle();
  }
  mock_server_->AddUpdateDirectory(1, 0, "foo", 2, 4, foreign_cache_guid(),
                                   "-1");
  mock_server_->SetLastUpdateDeleted();
  EXPECT_TRUE(SyncShareNudge());
  // This used to be rejected as it's an undeletion. Now, it results in moving
  // the delete path aside.
  mock_server_->AddUpdateDirectory(2, 1, "bar", 3, 5, foreign_cache_guid(),
                                   "-2");
  EXPECT_TRUE(SyncShareNudge());
  {
    syncable::ReadTransaction trans(FROM_HERE, directory());
    Entry entry(&trans, GET_BY_ID, ids_.FromNumber(2));
    ASSERT_TRUE(entry.good());
    EXPECT_TRUE(entry.GetIsDel());
    EXPECT_FALSE(entry.GetServerIsDel());
    EXPECT_TRUE(entry.GetIsUnappliedUpdate());
    EXPECT_NE(metahandle, entry.GetMetahandle());
  }
}

TEST_F(SyncerTest, DirectoryUpdateTest) {
  Id in_root_id = ids_.NewServerId();
  Id in_in_root_id = ids_.NewServerId();

  mock_server_->AddUpdateDirectory(in_root_id, TestIdFactory::root(),
                                   "in_root_name", 2, 2, foreign_cache_guid(),
                                   "-1");
  mock_server_->AddUpdateDirectory(in_in_root_id, in_root_id, "in_in_root_name",
                                   3, 3, foreign_cache_guid(), "-2");
  EXPECT_TRUE(SyncShareNudge());
  {
    syncable::ReadTransaction trans(FROM_HERE, directory());
    Entry in_root(&trans, GET_BY_ID, in_root_id);
    ASSERT_TRUE(in_root.good());
    EXPECT_EQ("in_root_name", in_root.GetNonUniqueName());
    EXPECT_EQ(TestIdFactory::root(), in_root.GetParentId());

    Entry in_in_root(&trans, GET_BY_ID, in_in_root_id);
    ASSERT_TRUE(in_in_root.good());
    EXPECT_EQ("in_in_root_name", in_in_root.GetNonUniqueName());
    EXPECT_EQ(in_root_id, in_in_root.GetParentId());
  }
}

TEST_F(SyncerTest, DirectoryCommitTest) {
  syncable::Id in_root_id, in_dir_id;
  int64_t foo_metahandle;
  int64_t bar_metahandle;

  {
    syncable::WriteTransaction wtrans(FROM_HERE, UNITTEST, directory());
    MutableEntry parent(&wtrans, CREATE, BOOKMARKS, root_id_, "foo");
    ASSERT_TRUE(parent.good());
    parent.PutIsUnsynced(true);
    parent.PutIsDir(true);
    parent.PutSpecifics(DefaultBookmarkSpecifics());
    in_root_id = parent.GetId();
    foo_metahandle = parent.GetMetahandle();

    MutableEntry child(&wtrans, CREATE, BOOKMARKS, parent.GetId(), "bar");
    ASSERT_TRUE(child.good());
    child.PutIsUnsynced(true);
    child.PutIsDir(true);
    child.PutSpecifics(DefaultBookmarkSpecifics());
    bar_metahandle = child.GetMetahandle();
    in_dir_id = parent.GetId();
  }
  EXPECT_TRUE(SyncShareNudge());
  {
    syncable::ReadTransaction trans(FROM_HERE, directory());
    Entry fail_by_old_id_entry(&trans, GET_BY_ID, in_root_id);
    ASSERT_FALSE(fail_by_old_id_entry.good());

    Entry foo_entry(&trans, GET_BY_HANDLE, foo_metahandle);
    ASSERT_TRUE(foo_entry.good());
    EXPECT_EQ("foo", foo_entry.GetNonUniqueName());
    EXPECT_NE(in_root_id, foo_entry.GetId());

    Entry bar_entry(&trans, GET_BY_HANDLE, bar_metahandle);
    ASSERT_TRUE(bar_entry.good());
    EXPECT_EQ("bar", bar_entry.GetNonUniqueName());
    EXPECT_NE(in_dir_id, bar_entry.GetId());
    EXPECT_EQ(foo_entry.GetId(), bar_entry.GetParentId());
  }
}

TEST_F(SyncerTest, TestClientCommandDuringUpdate) {
  using sync_pb::ClientCommand;

  auto command = std::make_unique<ClientCommand>();
  command->set_set_sync_poll_interval(8);
  command->set_set_sync_long_poll_interval(800);
  command->set_sessions_commit_delay_seconds(3141);
  sync_pb::CustomNudgeDelay* bookmark_delay =
      command->add_custom_nudge_delays();
  bookmark_delay->set_datatype_id(
      GetSpecificsFieldNumberFromModelType(BOOKMARKS));
  bookmark_delay->set_delay_ms(950);
  command->set_client_invalidation_hint_buffer_size(11);
  mock_server_->AddUpdateDirectory(1, 0, "in_root", 1, 1, foreign_cache_guid(),
                                   "-1");
  mock_server_->SetGUClientCommand(std::move(command));
  EXPECT_TRUE(SyncShareNudge());

  EXPECT_EQ(TimeDelta::FromSeconds(8), last_poll_interval_received_);
  EXPECT_EQ(TimeDelta::FromSeconds(3141), last_sessions_commit_delay_);
  EXPECT_EQ(TimeDelta::FromMilliseconds(950), last_bookmarks_commit_delay_);
  EXPECT_EQ(11, last_client_invalidation_hint_buffer_size_);

  command = std::make_unique<ClientCommand>();
  command->set_set_sync_poll_interval(180);
  command->set_set_sync_long_poll_interval(190);
  command->set_sessions_commit_delay_seconds(2718);
  bookmark_delay = command->add_custom_nudge_delays();
  bookmark_delay->set_datatype_id(
      GetSpecificsFieldNumberFromModelType(BOOKMARKS));
  bookmark_delay->set_delay_ms(1050);
  command->set_client_invalidation_hint_buffer_size(9);
  mock_server_->AddUpdateDirectory(1, 0, "in_root", 1, 1, foreign_cache_guid(),
                                   "-1");
  mock_server_->SetGUClientCommand(std::move(command));
  EXPECT_TRUE(SyncShareNudge());

  EXPECT_EQ(TimeDelta::FromSeconds(180), last_poll_interval_received_);
  EXPECT_EQ(TimeDelta::FromSeconds(2718), last_sessions_commit_delay_);
  EXPECT_EQ(TimeDelta::FromMilliseconds(1050), last_bookmarks_commit_delay_);
  EXPECT_EQ(9, last_client_invalidation_hint_buffer_size_);
}

TEST_F(SyncerTest, TestClientCommandDuringCommit) {
  using sync_pb::ClientCommand;

  auto command = std::make_unique<ClientCommand>();
  command->set_set_sync_poll_interval(8);
  command->set_set_sync_long_poll_interval(800);
  command->set_sessions_commit_delay_seconds(3141);
  sync_pb::CustomNudgeDelay* bookmark_delay =
      command->add_custom_nudge_delays();
  bookmark_delay->set_datatype_id(
      GetSpecificsFieldNumberFromModelType(BOOKMARKS));
  bookmark_delay->set_delay_ms(950);
  command->set_client_invalidation_hint_buffer_size(11);
  CreateUnsyncedDirectory("X", "id_X");
  mock_server_->SetCommitClientCommand(std::move(command));
  EXPECT_TRUE(SyncShareNudge());

  EXPECT_EQ(TimeDelta::FromSeconds(8), last_poll_interval_received_);
  EXPECT_EQ(TimeDelta::FromSeconds(3141), last_sessions_commit_delay_);
  EXPECT_EQ(TimeDelta::FromMilliseconds(950), last_bookmarks_commit_delay_);
  EXPECT_EQ(11, last_client_invalidation_hint_buffer_size_);

  command = std::make_unique<ClientCommand>();
  command->set_set_sync_poll_interval(180);
  command->set_set_sync_long_poll_interval(190);
  command->set_sessions_commit_delay_seconds(2718);
  bookmark_delay = command->add_custom_nudge_delays();
  bookmark_delay->set_datatype_id(
      GetSpecificsFieldNumberFromModelType(BOOKMARKS));
  bookmark_delay->set_delay_ms(1050);
  command->set_client_invalidation_hint_buffer_size(9);
  CreateUnsyncedDirectory("Y", "id_Y");
  mock_server_->SetCommitClientCommand(std::move(command));
  EXPECT_TRUE(SyncShareNudge());

  EXPECT_EQ(TimeDelta::FromSeconds(180), last_poll_interval_received_);
  EXPECT_EQ(TimeDelta::FromSeconds(2718), last_sessions_commit_delay_);
  EXPECT_EQ(TimeDelta::FromMilliseconds(1050), last_bookmarks_commit_delay_);
  EXPECT_EQ(9, last_client_invalidation_hint_buffer_size_);
}

TEST_F(SyncerTest, EnsureWeSendUpOldParent) {
  syncable::Id folder_one_id = ids_.FromNumber(1);
  syncable::Id folder_two_id = ids_.FromNumber(2);

  mock_server_->AddUpdateDirectory(folder_one_id, TestIdFactory::root(),
                                   "folder_one", 1, 1, foreign_cache_guid(),
                                   "-1");
  mock_server_->AddUpdateDirectory(folder_two_id, TestIdFactory::root(),
                                   "folder_two", 1, 1, foreign_cache_guid(),
                                   "-2");
  EXPECT_TRUE(SyncShareNudge());
  {
    // A moved entry should send an "old parent."
    syncable::WriteTransaction trans(FROM_HERE, UNITTEST, directory());
    MutableEntry entry(&trans, GET_BY_ID, folder_one_id);
    ASSERT_TRUE(entry.good());
    entry.PutParentId(folder_two_id);
    entry.PutIsUnsynced(true);
    // A new entry should send no "old parent."
    MutableEntry create(&trans, CREATE, BOOKMARKS, trans.root_id(),
                        "new_folder");
    create.PutIsUnsynced(true);
    create.PutSpecifics(DefaultBookmarkSpecifics());
  }
  EXPECT_TRUE(SyncShareNudge());
  const sync_pb::CommitMessage& commit = mock_server_->last_sent_commit();
  ASSERT_EQ(2, commit.entries_size());
  EXPECT_EQ("2", commit.entries(0).parent_id_string());
  EXPECT_EQ("0", commit.entries(0).old_parent_id());
  EXPECT_FALSE(commit.entries(1).has_old_parent_id());
}

TEST_F(SyncerTest, Test64BitVersionSupport) {
  int64_t really_big_int = std::numeric_limits<int64_t>::max() - 12;
  const string name("ringo's dang orang ran rings around my o-ring");
  int64_t item_metahandle;

  // Try writing max int64_t to the version fields of a meta entry.
  {
    syncable::WriteTransaction wtrans(FROM_HERE, UNITTEST, directory());
    MutableEntry entry(&wtrans, CREATE, BOOKMARKS, wtrans.root_id(), name);
    ASSERT_TRUE(entry.good());
    entry.PutBaseVersion(really_big_int);
    entry.PutServerVersion(really_big_int);
    entry.PutId(ids_.NewServerId());
    item_metahandle = entry.GetMetahandle();
  }
  // Now read it back out and make sure the value is max int64_t.
  syncable::ReadTransaction rtrans(FROM_HERE, directory());
  Entry entry(&rtrans, GET_BY_HANDLE, item_metahandle);
  ASSERT_TRUE(entry.good());
  EXPECT_EQ(really_big_int, entry.GetBaseVersion());
}

TEST_F(SyncerTest, TestSimpleUndelete) {
  Id id = ids_.MakeServer("undeletion item"), root = TestIdFactory::root();
  mock_server_->set_conflict_all_commits(true);
  // Let there be an entry from the server.
  mock_server_->AddUpdateBookmark(id, root, "foo", 1, 10, foreign_cache_guid(),
                                  "-1");
  EXPECT_TRUE(SyncShareNudge());
  // Check it out and delete it.
  {
    syncable::WriteTransaction wtrans(FROM_HERE, UNITTEST, directory());
    MutableEntry entry(&wtrans, GET_BY_ID, id);
    ASSERT_TRUE(entry.good());
    EXPECT_FALSE(entry.GetIsUnappliedUpdate());
    EXPECT_FALSE(entry.GetIsUnsynced());
    EXPECT_FALSE(entry.GetIsDel());
    // Delete it locally.
    entry.PutIsDel(true);
  }
  EXPECT_TRUE(SyncShareNudge());
  // Confirm we see IS_DEL and not SERVER_IS_DEL.
  {
    syncable::ReadTransaction trans(FROM_HERE, directory());
    Entry entry(&trans, GET_BY_ID, id);
    ASSERT_TRUE(entry.good());
    EXPECT_FALSE(entry.GetIsUnappliedUpdate());
    EXPECT_FALSE(entry.GetIsUnsynced());
    EXPECT_TRUE(entry.GetIsDel());
    EXPECT_FALSE(entry.GetServerIsDel());
  }
  EXPECT_TRUE(SyncShareNudge());
  // Update from server confirming deletion.
  mock_server_->AddUpdateBookmark(id, root, "foo", 2, 11, foreign_cache_guid(),
                                  "-1");
  mock_server_->SetLastUpdateDeleted();
  EXPECT_TRUE(SyncShareNudge());
  // IS_DEL AND SERVER_IS_DEL now both true.
  {
    syncable::ReadTransaction trans(FROM_HERE, directory());
    Entry entry(&trans, GET_BY_ID, id);
    ASSERT_TRUE(entry.good());
    EXPECT_FALSE(entry.GetIsUnappliedUpdate());
    EXPECT_FALSE(entry.GetIsUnsynced());
    EXPECT_TRUE(entry.GetIsDel());
    EXPECT_TRUE(entry.GetServerIsDel());
  }
  // Undelete from server.
  mock_server_->AddUpdateBookmark(id, root, "foo", 2, 12, foreign_cache_guid(),
                                  "-1");
  EXPECT_TRUE(SyncShareNudge());
  // IS_DEL and SERVER_IS_DEL now both false.
  {
    syncable::ReadTransaction trans(FROM_HERE, directory());
    Entry entry(&trans, GET_BY_ID, id);
    ASSERT_TRUE(entry.good());
    EXPECT_FALSE(entry.GetIsUnappliedUpdate());
    EXPECT_FALSE(entry.GetIsUnsynced());
    EXPECT_FALSE(entry.GetIsDel());
    EXPECT_FALSE(entry.GetServerIsDel());
  }
}

TEST_F(SyncerTest, TestUndeleteWithMissingDeleteUpdate) {
  Id id = ids_.MakeServer("undeletion item"), root = TestIdFactory::root();
  // Let there be a entry, from the server.
  mock_server_->set_conflict_all_commits(true);
  mock_server_->AddUpdateBookmark(id, root, "foo", 1, 10, foreign_cache_guid(),
                                  "-1");
  EXPECT_TRUE(SyncShareNudge());
  // Check it out and delete it.
  {
    syncable::WriteTransaction wtrans(FROM_HERE, UNITTEST, directory());
    MutableEntry entry(&wtrans, GET_BY_ID, id);
    ASSERT_TRUE(entry.good());
    EXPECT_FALSE(entry.GetIsUnappliedUpdate());
    EXPECT_FALSE(entry.GetIsUnsynced());
    EXPECT_FALSE(entry.GetIsDel());
    // Delete it locally.
    entry.PutIsDel(true);
  }
  EXPECT_TRUE(SyncShareNudge());
  // Confirm we see IS_DEL and not SERVER_IS_DEL.
  {
    syncable::ReadTransaction trans(FROM_HERE, directory());
    Entry entry(&trans, GET_BY_ID, id);
    ASSERT_TRUE(entry.good());
    EXPECT_FALSE(entry.GetIsUnappliedUpdate());
    EXPECT_FALSE(entry.GetIsUnsynced());
    EXPECT_TRUE(entry.GetIsDel());
    EXPECT_FALSE(entry.GetServerIsDel());
  }
  EXPECT_TRUE(SyncShareNudge());
  // Say we do not get an update from server confirming deletion. Undelete
  // from server
  mock_server_->AddUpdateBookmark(id, root, "foo", 2, 12, foreign_cache_guid(),
                                  "-1");
  EXPECT_TRUE(SyncShareNudge());
  // IS_DEL and SERVER_IS_DEL now both false.
  {
    syncable::ReadTransaction trans(FROM_HERE, directory());
    Entry entry(&trans, GET_BY_ID, id);
    ASSERT_TRUE(entry.good());
    EXPECT_FALSE(entry.GetIsUnappliedUpdate());
    EXPECT_FALSE(entry.GetIsUnsynced());
    EXPECT_FALSE(entry.GetIsDel());
    EXPECT_FALSE(entry.GetServerIsDel());
  }
}

TEST_F(SyncerTest, TestUndeleteIgnoreCorrectlyUnappliedUpdate) {
  Id id1 = ids_.MakeServer("first"), id2 = ids_.MakeServer("second");
  Id root = TestIdFactory::root();
  // Duplicate! expect path clashing!
  mock_server_->set_conflict_all_commits(true);
  mock_server_->AddUpdateBookmark(id1, root, "foo", 1, 10, foreign_cache_guid(),
                                  "-1");
  mock_server_->AddUpdateBookmark(id2, root, "foo", 1, 10, foreign_cache_guid(),
                                  "-2");
  EXPECT_TRUE(SyncShareNudge());
  mock_server_->AddUpdateBookmark(id2, root, "foo2", 2, 20,
                                  foreign_cache_guid(), "-2");
  EXPECT_TRUE(SyncShareNudge());  // Now just don't explode.
}

TEST_F(SyncerTest, ClientTagServerCreatedUpdatesWork) {
  mock_server_->AddUpdateDirectory(1, 0, "permitem1", 1, 10,
                                   foreign_cache_guid(), "-1");
  mock_server_->SetLastUpdateClientTag("permfolder");

  EXPECT_TRUE(SyncShareNudge());

  {
    syncable::ReadTransaction trans(FROM_HERE, directory());
    Entry perm_folder(&trans, GET_BY_CLIENT_TAG, "permfolder");
    ASSERT_TRUE(perm_folder.good());
    EXPECT_FALSE(perm_folder.GetIsDel());
    EXPECT_FALSE(perm_folder.GetIsUnappliedUpdate());
    EXPECT_FALSE(perm_folder.GetIsUnsynced());
    EXPECT_EQ("permfolder", perm_folder.GetUniqueClientTag());
    EXPECT_EQ("permitem1", perm_folder.GetNonUniqueName());
  }

  mock_server_->AddUpdateDirectory(1, 0, "permitem_renamed", 10, 100,
                                   foreign_cache_guid(), "-1");
  mock_server_->SetLastUpdateClientTag("permfolder");
  EXPECT_TRUE(SyncShareNudge());

  {
    syncable::ReadTransaction trans(FROM_HERE, directory());

    Entry perm_folder(&trans, GET_BY_CLIENT_TAG, "permfolder");
    ASSERT_TRUE(perm_folder.good());
    EXPECT_FALSE(perm_folder.GetIsDel());
    EXPECT_FALSE(perm_folder.GetIsUnappliedUpdate());
    EXPECT_FALSE(perm_folder.GetIsUnsynced());
    EXPECT_EQ("permfolder", perm_folder.GetUniqueClientTag());
    EXPECT_EQ("permitem_renamed", perm_folder.GetNonUniqueName());
  }
}

TEST_F(SyncerTest, ClientTagIllegalUpdateIgnored) {
  mock_server_->AddUpdateDirectory(1, 0, "permitem1", 1, 10,
                                   foreign_cache_guid(), "-1");
  mock_server_->SetLastUpdateClientTag("permfolder");

  EXPECT_TRUE(SyncShareNudge());

  {
    syncable::ReadTransaction trans(FROM_HERE, directory());
    Entry perm_folder(&trans, GET_BY_CLIENT_TAG, "permfolder");
    ASSERT_TRUE(perm_folder.good());
    EXPECT_FALSE(perm_folder.GetIsUnappliedUpdate());
    EXPECT_FALSE(perm_folder.GetIsUnsynced());
    EXPECT_EQ("permfolder", perm_folder.GetUniqueClientTag());
    EXPECT_EQ("permitem1", perm_folder.GetNonUniqueName());
    EXPECT_TRUE(perm_folder.GetId().ServerKnows());
  }

  mock_server_->AddUpdateDirectory(1, 0, "permitem_renamed", 10, 100,
                                   foreign_cache_guid(), "-1");
  mock_server_->SetLastUpdateClientTag("wrongtag");
  EXPECT_TRUE(SyncShareNudge());

  {
    syncable::ReadTransaction trans(FROM_HERE, directory());

    // This update is rejected because it has the same ID, but a
    // different tag than one that is already on the client.
    // The client has a ServerKnows ID, which cannot be overwritten.
    Entry rejected_update(&trans, GET_BY_CLIENT_TAG, "wrongtag");
    EXPECT_FALSE(rejected_update.good());

    Entry perm_folder(&trans, GET_BY_CLIENT_TAG, "permfolder");
    ASSERT_TRUE(perm_folder.good());
    EXPECT_FALSE(perm_folder.GetIsUnappliedUpdate());
    EXPECT_FALSE(perm_folder.GetIsUnsynced());
    EXPECT_EQ("permitem1", perm_folder.GetNonUniqueName());
  }
}

TEST_F(SyncerTest, ClientTagUncommittedTagMatchesUpdate) {
  int64_t original_metahandle = 0;

  {
    syncable::WriteTransaction trans(FROM_HERE, UNITTEST, directory());
    MutableEntry pref(&trans, CREATE, PREFERENCES, ids_.root(), "name");
    ASSERT_TRUE(pref.good());
    pref.PutUniqueClientTag("tag");
    pref.PutIsUnsynced(true);
    EXPECT_FALSE(pref.GetIsUnappliedUpdate());
    EXPECT_FALSE(pref.GetId().ServerKnows());
    original_metahandle = pref.GetMetahandle();
  }

  syncable::Id server_id = TestIdFactory::MakeServer("id");
  mock_server_->AddUpdatePref(server_id.GetServerId(),
                              ids_.root().GetServerId(), "tag", 10, 100);
  mock_server_->set_conflict_all_commits(true);

  EXPECT_FALSE(SyncShareNudge());
  // This should cause client tag reunion, preserving the metahandle.
  {
    syncable::ReadTransaction trans(FROM_HERE, directory());

    Entry pref(&trans, GET_BY_CLIENT_TAG, "tag");
    ASSERT_TRUE(pref.good());
    EXPECT_FALSE(pref.GetIsDel());
    EXPECT_FALSE(pref.GetIsUnappliedUpdate());
    EXPECT_TRUE(pref.GetIsUnsynced());
    EXPECT_EQ(10, pref.GetBaseVersion());
    // Entry should have been given the new ID while preserving the
    // metahandle; client should have won the conflict resolution.
    EXPECT_EQ(original_metahandle, pref.GetMetahandle());
    EXPECT_EQ("tag", pref.GetUniqueClientTag());
    EXPECT_TRUE(pref.GetId().ServerKnows());
  }

  mock_server_->set_conflict_all_commits(false);
  EXPECT_TRUE(SyncShareNudge());

  // The resolved entry ought to commit cleanly.
  {
    syncable::ReadTransaction trans(FROM_HERE, directory());

    Entry pref(&trans, GET_BY_CLIENT_TAG, "tag");
    ASSERT_TRUE(pref.good());
    EXPECT_FALSE(pref.GetIsDel());
    EXPECT_FALSE(pref.GetIsUnappliedUpdate());
    EXPECT_FALSE(pref.GetIsUnsynced());
    EXPECT_LT(10, pref.GetBaseVersion());
    // Entry should have been given the new ID while preserving the
    // metahandle; client should have won the conflict resolution.
    EXPECT_EQ(original_metahandle, pref.GetMetahandle());
    EXPECT_EQ("tag", pref.GetUniqueClientTag());
    EXPECT_TRUE(pref.GetId().ServerKnows());
  }
}

TEST_F(SyncerTest, ClientTagConflictWithDeletedLocalEntry) {
  {
    // Create a deleted local entry with a unique client tag.
    syncable::WriteTransaction trans(FROM_HERE, UNITTEST, directory());
    MutableEntry pref(&trans, CREATE, PREFERENCES, ids_.root(), "name");
    ASSERT_TRUE(pref.good());
    ASSERT_FALSE(pref.GetId().ServerKnows());
    pref.PutUniqueClientTag("tag");
    pref.PutIsUnsynced(true);

    // Note: IS_DEL && !ServerKnows() will clear the UNSYNCED bit.
    // (We never attempt to commit server-unknown deleted items, so this
    // helps us clean up those entries).
    pref.PutIsDel(true);
  }

  // Prepare an update with the same unique client tag.
  syncable::Id server_id = TestIdFactory::MakeServer("id");
  mock_server_->AddUpdatePref(server_id.GetServerId(),
                              ids_.root().GetServerId(), "tag", 10, 100);

  EXPECT_TRUE(SyncShareNudge());
  // The local entry will be overwritten.
  {
    syncable::ReadTransaction trans(FROM_HERE, directory());

    Entry pref(&trans, GET_BY_CLIENT_TAG, "tag");
    ASSERT_TRUE(pref.good());
    ASSERT_TRUE(pref.GetId().ServerKnows());
    EXPECT_FALSE(pref.GetIsDel());
    EXPECT_FALSE(pref.GetIsUnappliedUpdate());
    EXPECT_FALSE(pref.GetIsUnsynced());
    EXPECT_EQ(10, pref.GetBaseVersion());
    EXPECT_EQ("tag", pref.GetUniqueClientTag());
  }
}

TEST_F(SyncerTest, ClientTagUpdateClashesWithLocalEntry) {
  // This test is written assuming that ID comparison
  // will work out in a particular way.
  EXPECT_LT(ids_.FromNumber(1), ids_.FromNumber(2));
  EXPECT_LT(ids_.FromNumber(3), ids_.FromNumber(4));

  syncable::Id id1 = TestIdFactory::MakeServer("1");
  mock_server_->AddUpdatePref(id1.GetServerId(), "", "tag1", 10, 100);

  syncable::Id id4 = TestIdFactory::MakeServer("4");
  mock_server_->AddUpdatePref(id4.GetServerId(), "", "tag2", 11, 110);

  mock_server_->set_conflict_all_commits(true);

  EXPECT_TRUE(SyncShareNudge());
  int64_t tag1_metahandle = syncable::kInvalidMetaHandle;
  int64_t tag2_metahandle = syncable::kInvalidMetaHandle;
  // This should cause client tag overwrite.
  {
    syncable::ReadTransaction trans(FROM_HERE, directory());

    Entry tag1(&trans, GET_BY_CLIENT_TAG, "tag1");
    ASSERT_TRUE(tag1.good());
    ASSERT_TRUE(tag1.GetId().ServerKnows());
    ASSERT_EQ(id1, tag1.GetId());
    EXPECT_FALSE(tag1.GetIsDel());
    EXPECT_FALSE(tag1.GetIsUnappliedUpdate());
    EXPECT_FALSE(tag1.GetIsUnsynced());
    EXPECT_EQ(10, tag1.GetBaseVersion());
    EXPECT_EQ("tag1", tag1.GetUniqueClientTag());
    tag1_metahandle = tag1.GetMetahandle();

    Entry tag2(&trans, GET_BY_CLIENT_TAG, "tag2");
    ASSERT_TRUE(tag2.good());
    ASSERT_TRUE(tag2.GetId().ServerKnows());
    ASSERT_EQ(id4, tag2.GetId());
    EXPECT_FALSE(tag2.GetIsDel());
    EXPECT_FALSE(tag2.GetIsUnappliedUpdate());
    EXPECT_FALSE(tag2.GetIsUnsynced());
    EXPECT_EQ(11, tag2.GetBaseVersion());
    EXPECT_EQ("tag2", tag2.GetUniqueClientTag());
    tag2_metahandle = tag2.GetMetahandle();

    // Preferences type root should have been created by the updates above.
    ASSERT_TRUE(directory()->InitialSyncEndedForType(&trans, PREFERENCES));

    Entry pref_root(&trans, GET_TYPE_ROOT, PREFERENCES);
    ASSERT_TRUE(pref_root.good());

    Directory::Metahandles children;
    directory()->GetChildHandlesById(&trans, pref_root.GetId(), &children);
    ASSERT_EQ(2U, children.size());
  }

  syncable::Id id2 = TestIdFactory::MakeServer("2");
  mock_server_->AddUpdatePref(id2.GetServerId(), "", "tag1", 12, 120);
  syncable::Id id3 = TestIdFactory::MakeServer("3");
  mock_server_->AddUpdatePref(id3.GetServerId(), "", "tag2", 13, 130);
  EXPECT_TRUE(SyncShareNudge());

  {
    syncable::ReadTransaction trans(FROM_HERE, directory());

    Entry tag1(&trans, GET_BY_CLIENT_TAG, "tag1");
    ASSERT_TRUE(tag1.good());
    ASSERT_TRUE(tag1.GetId().ServerKnows());
    ASSERT_EQ(id1, tag1.GetId())
        << "ID 1 should be kept, since it was less than ID 2.";
    EXPECT_FALSE(tag1.GetIsDel());
    EXPECT_FALSE(tag1.GetIsUnappliedUpdate());
    EXPECT_FALSE(tag1.GetIsUnsynced());
    EXPECT_EQ(10, tag1.GetBaseVersion());
    EXPECT_EQ("tag1", tag1.GetUniqueClientTag());
    EXPECT_EQ(tag1_metahandle, tag1.GetMetahandle());

    Entry tag2(&trans, GET_BY_CLIENT_TAG, "tag2");
    ASSERT_TRUE(tag2.good());
    ASSERT_TRUE(tag2.GetId().ServerKnows());
    ASSERT_EQ(id3, tag2.GetId())
        << "ID 3 should be kept, since it was less than ID 4.";
    EXPECT_FALSE(tag2.GetIsDel());
    EXPECT_FALSE(tag2.GetIsUnappliedUpdate());
    EXPECT_FALSE(tag2.GetIsUnsynced());
    EXPECT_EQ(13, tag2.GetBaseVersion());
    EXPECT_EQ("tag2", tag2.GetUniqueClientTag());
    EXPECT_EQ(tag2_metahandle, tag2.GetMetahandle());

    // Preferences type root should have been created by the updates above.
    ASSERT_TRUE(directory()->InitialSyncEndedForType(&trans, PREFERENCES));

    Entry pref_root(&trans, GET_TYPE_ROOT, PREFERENCES);
    ASSERT_TRUE(pref_root.good());

    Directory::Metahandles children;
    directory()->GetChildHandlesById(&trans, pref_root.GetId(), &children);
    ASSERT_EQ(2U, children.size());
  }
}

TEST_F(SyncerTest, ClientTagClashWithinBatchOfUpdates) {
  // This test is written assuming that ID comparison
  // will work out in a particular way.
  EXPECT_LT(ids_.FromNumber(1), ids_.FromNumber(4));
  EXPECT_LT(ids_.FromNumber(201), ids_.FromNumber(205));

  // Least ID: winner.
  mock_server_->AddUpdatePref(ids_.FromNumber(1).GetServerId(), "", "tag a", 1,
                              10);
  mock_server_->AddUpdatePref(ids_.FromNumber(2).GetServerId(), "", "tag a", 11,
                              110);
  mock_server_->AddUpdatePref(ids_.FromNumber(3).GetServerId(), "", "tag a", 12,
                              120);
  mock_server_->AddUpdatePref(ids_.FromNumber(4).GetServerId(), "", "tag a", 13,
                              130);
  mock_server_->AddUpdatePref(ids_.FromNumber(105).GetServerId(), "", "tag b",
                              14, 140);
  mock_server_->AddUpdatePref(ids_.FromNumber(102).GetServerId(), "", "tag b",
                              15, 150);
  // Least ID: winner.
  mock_server_->AddUpdatePref(ids_.FromNumber(101).GetServerId(), "", "tag b",
                              16, 160);
  mock_server_->AddUpdatePref(ids_.FromNumber(104).GetServerId(), "", "tag b",
                              17, 170);

  mock_server_->AddUpdatePref(ids_.FromNumber(205).GetServerId(), "", "tag c",
                              18, 180);
  mock_server_->AddUpdatePref(ids_.FromNumber(202).GetServerId(), "", "tag c",
                              19, 190);
  mock_server_->AddUpdatePref(ids_.FromNumber(204).GetServerId(), "", "tag c",
                              20, 200);
  // Least ID: winner.
  mock_server_->AddUpdatePref(ids_.FromNumber(201).GetServerId(), "", "tag c",
                              21, 210);

  mock_server_->set_conflict_all_commits(true);

  EXPECT_TRUE(SyncShareNudge());
  // This should cause client tag overwrite.
  {
    syncable::ReadTransaction trans(FROM_HERE, directory());

    Entry tag_a(&trans, GET_BY_CLIENT_TAG, "tag a");
    ASSERT_TRUE(tag_a.good());
    EXPECT_TRUE(tag_a.GetId().ServerKnows());
    EXPECT_EQ(ids_.FromNumber(1), tag_a.GetId());
    EXPECT_FALSE(tag_a.GetIsDel());
    EXPECT_FALSE(tag_a.GetIsUnappliedUpdate());
    EXPECT_FALSE(tag_a.GetIsUnsynced());
    EXPECT_EQ(1, tag_a.GetBaseVersion());
    EXPECT_EQ("tag a", tag_a.GetUniqueClientTag());

    Entry tag_b(&trans, GET_BY_CLIENT_TAG, "tag b");
    ASSERT_TRUE(tag_b.good());
    EXPECT_TRUE(tag_b.GetId().ServerKnows());
    EXPECT_EQ(ids_.FromNumber(101), tag_b.GetId());
    EXPECT_FALSE(tag_b.GetIsDel());
    EXPECT_FALSE(tag_b.GetIsUnappliedUpdate());
    EXPECT_FALSE(tag_b.GetIsUnsynced());
    EXPECT_EQ(16, tag_b.GetBaseVersion());
    EXPECT_EQ("tag b", tag_b.GetUniqueClientTag());

    Entry tag_c(&trans, GET_BY_CLIENT_TAG, "tag c");
    ASSERT_TRUE(tag_c.good());
    EXPECT_TRUE(tag_c.GetId().ServerKnows());
    EXPECT_EQ(ids_.FromNumber(201), tag_c.GetId());
    EXPECT_FALSE(tag_c.GetIsDel());
    EXPECT_FALSE(tag_c.GetIsUnappliedUpdate());
    EXPECT_FALSE(tag_c.GetIsUnsynced());
    EXPECT_EQ(21, tag_c.GetBaseVersion());
    EXPECT_EQ("tag c", tag_c.GetUniqueClientTag());

    // Preferences type root should have been created by the updates above.
    ASSERT_TRUE(directory()->InitialSyncEndedForType(&trans, PREFERENCES));

    Entry pref_root(&trans, GET_TYPE_ROOT, PREFERENCES);
    ASSERT_TRUE(pref_root.good());

    // Verify that we have exactly 3 tagged nodes under the type root.
    Directory::Metahandles children;
    directory()->GetChildHandlesById(&trans, pref_root.GetId(), &children);
    ASSERT_EQ(3U, children.size());
  }
}

TEST_F(SyncerTest, GetUpdatesSetsRequestedTypes) {
  // The expectations of this test happen in the MockConnectionManager's
  // GetUpdates handler.  EnableDatatype sets the expectation value from our
  // set of enabled/disabled datatypes.
  EXPECT_TRUE(SyncShareNudge());
  EXPECT_EQ(1, mock_server_->GetAndClearNumGetUpdatesRequests());

  EnableDatatype(AUTOFILL);
  EXPECT_TRUE(SyncShareNudge());
  EXPECT_EQ(1, mock_server_->GetAndClearNumGetUpdatesRequests());

  DisableDatatype(BOOKMARKS);
  EXPECT_TRUE(SyncShareNudge());
  EXPECT_EQ(1, mock_server_->GetAndClearNumGetUpdatesRequests());

  DisableDatatype(AUTOFILL);
  EXPECT_TRUE(SyncShareNudge());
  EXPECT_EQ(1, mock_server_->GetAndClearNumGetUpdatesRequests());

  DisableDatatype(PREFERENCES);
  EnableDatatype(AUTOFILL);
  EXPECT_TRUE(SyncShareNudge());
  EXPECT_EQ(1, mock_server_->GetAndClearNumGetUpdatesRequests());
}

// A typical scenario: server and client each have one update for the other.
// This is the "happy path" alternative to UpdateFailsThenDontCommit.
TEST_F(SyncerTest, UpdateThenCommit) {
  syncable::Id to_receive = ids_.NewServerId();
  syncable::Id to_commit = ids_.NewLocalId();

  mock_server_->AddUpdateDirectory(to_receive, ids_.root(), "x", 1, 10,
                                   foreign_cache_guid(), "-1");
  int64_t commit_handle = CreateUnsyncedDirectory("y", to_commit);
  EXPECT_TRUE(SyncShareNudge());

  // The sync cycle should have included a GetUpdate, then a commit.  By the
  // time the commit happened, we should have known for sure that there were no
  // hierarchy conflicts, and reported this fact to the server.
  ASSERT_TRUE(mock_server_->last_request().has_commit());
  VerifyNoHierarchyConflictsReported(mock_server_->last_request());

  syncable::ReadTransaction trans(FROM_HERE, directory());

  Entry received(&trans, GET_BY_ID, to_receive);
  ASSERT_TRUE(received.good());
  EXPECT_FALSE(received.GetIsUnsynced());
  EXPECT_FALSE(received.GetIsUnappliedUpdate());

  Entry committed(&trans, GET_BY_HANDLE, commit_handle);
  ASSERT_TRUE(committed.good());
  EXPECT_FALSE(committed.GetIsUnsynced());
  EXPECT_FALSE(committed.GetIsUnappliedUpdate());
}

// Same as above, but this time we fail to download updates.
// We should not attempt to commit anything unless we successfully downloaded
// updates, otherwise we risk causing a server-side conflict.
TEST_F(SyncerTest, UpdateFailsThenDontCommit) {
  syncable::Id to_receive = ids_.NewServerId();
  syncable::Id to_commit = ids_.NewLocalId();

  mock_server_->AddUpdateDirectory(to_receive, ids_.root(), "x", 1, 10,
                                   foreign_cache_guid(), "-1");
  int64_t commit_handle = CreateUnsyncedDirectory("y", to_commit);
  mock_server_->FailNextPostBufferToPathCall();
  EXPECT_FALSE(SyncShareNudge());

  syncable::ReadTransaction trans(FROM_HERE, directory());

  // We did not receive this update.
  Entry received(&trans, GET_BY_ID, to_receive);
  ASSERT_FALSE(received.good());

  // And our local update remains unapplied.
  Entry committed(&trans, GET_BY_HANDLE, commit_handle);
  ASSERT_TRUE(committed.good());
  EXPECT_TRUE(committed.GetIsUnsynced());
  EXPECT_FALSE(committed.GetIsUnappliedUpdate());

  // Inform the Mock we won't be fetching all updates.
  mock_server_->ClearUpdatesQueue();
}

// Downloads two updates and applies them successfully.
// This is the "happy path" alternative to ConfigureFailsDontApplyUpdates.
TEST_F(SyncerTest, ConfigureDownloadsTwoBatchesSuccess) {
  syncable::Id node1 = ids_.NewServerId();
  syncable::Id node2 = ids_.NewServerId();

  // Construct the first GetUpdates response.
  mock_server_->AddUpdatePref(node1.GetServerId(), "", "one", 1, 10);
  mock_server_->SetChangesRemaining(1);
  mock_server_->NextUpdateBatch();

  // Construct the second GetUpdates response.
  mock_server_->AddUpdatePref(node2.GetServerId(), "", "two", 2, 20);

  SyncShareConfigure();

  // The type should now be marked as having the initial sync completed.
  EXPECT_TRUE(directory()->InitialSyncEndedForType(PREFERENCES));

  syncable::ReadTransaction trans(FROM_HERE, directory());
  // Both nodes should be downloaded and applied.

  Entry n1(&trans, GET_BY_ID, node1);
  ASSERT_TRUE(n1.good());
  EXPECT_FALSE(n1.GetIsUnappliedUpdate());

  Entry n2(&trans, GET_BY_ID, node2);
  ASSERT_TRUE(n2.good());
  EXPECT_FALSE(n2.GetIsUnappliedUpdate());
}

// Same as the above case, but this time the second batch fails to download.
TEST_F(SyncerTest, ConfigureFailsDontApplyUpdates) {
  syncable::Id node1 = ids_.NewServerId();
  syncable::Id node2 = ids_.NewServerId();

  // The scenario: we have two batches of updates with one update each.  A
  // normal confgure step would download all the updates one batch at a time and
  // apply them.  This configure will succeed in downloading the first batch
  // then fail when downloading the second.
  mock_server_->FailNthPostBufferToPathCall(2);

  // Construct the first GetUpdates response.
  mock_server_->AddUpdatePref(node1.GetServerId(), "", "one", 1, 10);
  mock_server_->SetChangesRemaining(1);
  mock_server_->NextUpdateBatch();

  // Construct the second GetUpdates response.
  mock_server_->AddUpdatePref(node2.GetServerId(), "", "two", 2, 20);

  SyncShareConfigure();

  // The type shouldn't be marked as having the initial sync completed.
  EXPECT_FALSE(directory()->InitialSyncEndedForType(PREFERENCES));

  syncable::ReadTransaction trans(FROM_HERE, directory());

  // The first node was downloaded, but not applied.
  Entry n1(&trans, GET_BY_ID, node1);
  ASSERT_TRUE(n1.good());
  EXPECT_TRUE(n1.GetIsUnappliedUpdate());

  // The second node was not downloaded.
  Entry n2(&trans, GET_BY_ID, node2);
  EXPECT_FALSE(n2.good());

  // One update remains undownloaded.
  mock_server_->ClearUpdatesQueue();
}

// Tests that if type is not registered with ModelTypeRegistry (e.g. because
// type's LoadModels failed), Syncer::ConfigureSyncShare runs without triggering
// DCHECK.
TEST_F(SyncerTest, ConfigureFailedUnregisteredType) {
  // Simulate type being unregistered before configuration by including type
  // that isn't registered with ModelTypeRegistry.
  SyncShareConfigureTypes(ModelTypeSet(APPS));

  // No explicit verification, DCHECK shouldn't have been triggered.
}

TEST_F(SyncerTest, GetKeySuccess) {
  KeystoreKeysHandler* keystore_keys_handler =
      model_type_registry_->keystore_keys_handler();
  EXPECT_TRUE(keystore_keys_handler->NeedKeystoreKey());

  SyncShareConfigure();

  EXPECT_EQ(SyncerError::SYNCER_OK,
            cycle_->status_controller().last_get_key_result().value());
  EXPECT_FALSE(keystore_keys_handler->NeedKeystoreKey());
}

TEST_F(SyncerTest, GetKeyEmpty) {
  KeystoreKeysHandler* keystore_keys_handler =
      model_type_registry_->keystore_keys_handler();
  EXPECT_TRUE(keystore_keys_handler->NeedKeystoreKey());

  mock_server_->SetKeystoreKey(std::string());
  SyncShareConfigure();

  EXPECT_NE(SyncerError::SYNCER_OK,
            cycle_->status_controller().last_get_key_result().value());
  EXPECT_TRUE(keystore_keys_handler->NeedKeystoreKey());
}

// Trigger an update that contains a progress marker only and verify that
// the type's permanent folder is created and the type is marked as having
// initial sync complete.
TEST_F(SyncerTest, ProgressMarkerOnlyUpdateCreatesRootFolder) {
  EXPECT_FALSE(directory()->InitialSyncEndedForType(PREFERENCES));
  sync_pb::DataTypeProgressMarker* marker =
      mock_server_->AddUpdateProgressMarker();
  marker->set_data_type_id(GetSpecificsFieldNumberFromModelType(PREFERENCES));
  marker->set_token("foobar");

  SyncShareNudge();

  {
    syncable::ReadTransaction trans(FROM_HERE, directory());
    Entry root(&trans, GET_TYPE_ROOT, PREFERENCES);
    EXPECT_TRUE(root.good());
  }

  EXPECT_TRUE(directory()->InitialSyncEndedForType(PREFERENCES));
}

// Verify that commit only types are never requested in GetUpdates, but still
// make it into the commit messages. Additionally, make sure failing GU types
// are correctly removed before commit.
TEST_F(SyncerTest, CommitOnlyTypes) {
  mock_server_->set_partial_failure(true);
  mock_server_->SetPartialFailureTypes(ModelTypeSet(PREFERENCES));

  EnableDatatype(USER_EVENTS);
  {
    syncable::WriteTransaction trans(FROM_HERE, UNITTEST, directory());

    MutableEntry pref(&trans, CREATE, PREFERENCES, ids_.root(), "name");
    ASSERT_TRUE(pref.good());
    pref.PutUniqueClientTag("tag1");
    pref.PutIsUnsynced(true);

    MutableEntry ext(&trans, CREATE, EXTENSIONS, ids_.root(), "name");
    ASSERT_TRUE(ext.good());
    ext.PutUniqueClientTag("tag2");
    ext.PutIsUnsynced(true);

    MutableEntry event(&trans, CREATE, USER_EVENTS, ids_.root(), "name");
    ASSERT_TRUE(event.good());
    event.PutUniqueClientTag("tag3");
    event.PutIsUnsynced(true);
  }

  EXPECT_TRUE(SyncShareNudge());

  ASSERT_EQ(2U, mock_server_->requests().size());
  ASSERT_TRUE(mock_server_->requests()[0].has_get_updates());
  // MockConnectionManager will ensure USER_EVENTS was not included in the GU.
  EXPECT_EQ(
      4, mock_server_->requests()[0].get_updates().from_progress_marker_size());

  ASSERT_TRUE(mock_server_->requests()[1].has_commit());
  const sync_pb::CommitMessage commit = mock_server_->requests()[1].commit();
  EXPECT_EQ(2, commit.entries_size());
  EXPECT_TRUE(commit.entries(0).specifics().has_extension());
  EXPECT_TRUE(commit.entries(1).specifics().has_user_event());
}

// Test what happens if a client deletes, then recreates, an object very
// quickly.  It is possible that the deletion gets sent as a commit, and
// the undelete happens during the commit request.  The principle here
// is that with a single committing client, conflicts should never
// be encountered, and a client encountering its past actions during
// getupdates should never feed back to override later actions.
//
// In cases of ordering A-F below, the outcome should be the same.
//   Exercised by UndeleteDuringCommit:
//     A. Delete - commit - undelete - commitresponse.
//     B. Delete - commit - undelete - commitresponse - getupdates.
//   Exercised by UndeleteBeforeCommit:
//     C. Delete - undelete - commit - commitresponse.
//     D. Delete - undelete - commit - commitresponse - getupdates.
//   Exercised by UndeleteAfterCommit:
//     E. Delete - commit - commitresponse - undelete - commit
//        - commitresponse.
//     F. Delete - commit - commitresponse - undelete - commit -
//        - commitresponse - getupdates.
class SyncerUndeletionTest : public SyncerTest {
 public:
  SyncerUndeletionTest()
      : client_tag_("foobar"), metahandle_(syncable::kInvalidMetaHandle) {}

  void Create() {
    syncable::WriteTransaction trans(FROM_HERE, UNITTEST, directory());
    MutableEntry perm_folder(&trans, CREATE, PREFERENCES, ids_.root(),
                             "clientname");
    ASSERT_TRUE(perm_folder.good());
    perm_folder.PutUniqueClientTag(client_tag_);
    perm_folder.PutIsUnsynced(true);
    if (perm_folder.GetSyncing())
      perm_folder.PutDirtySync(true);
    perm_folder.PutSpecifics(DefaultPreferencesSpecifics());
    EXPECT_FALSE(perm_folder.GetIsUnappliedUpdate());
    EXPECT_FALSE(perm_folder.GetId().ServerKnows());
    metahandle_ = perm_folder.GetMetahandle();
    local_id_ = perm_folder.GetId();
  }

  void Delete() {
    syncable::WriteTransaction trans(FROM_HERE, UNITTEST, directory());
    MutableEntry entry(&trans, GET_BY_CLIENT_TAG, client_tag_);
    ASSERT_TRUE(entry.good());
    EXPECT_EQ(metahandle_, entry.GetMetahandle());
    // The order of setting IS_UNSYNCED vs IS_DEL matters. See
    // WriteNode::Tombstone().
    entry.PutIsUnsynced(true);
    if (entry.GetSyncing())
      entry.PutDirtySync(true);
    entry.PutIsDel(true);
  }

  void Undelete() {
    syncable::WriteTransaction trans(FROM_HERE, UNITTEST, directory());
    MutableEntry entry(&trans, GET_BY_CLIENT_TAG, client_tag_);
    ASSERT_TRUE(entry.good());
    EXPECT_EQ(metahandle_, entry.GetMetahandle());
    EXPECT_TRUE(entry.GetIsDel());
    entry.PutIsDel(false);
    entry.PutIsUnsynced(true);
    if (entry.GetSyncing())
      entry.PutDirtySync(true);
  }

  int64_t GetMetahandleOfTag() {
    syncable::ReadTransaction trans(FROM_HERE, directory());
    Entry entry(&trans, GET_BY_CLIENT_TAG, client_tag_);
    EXPECT_TRUE(entry.good());
    if (!entry.good()) {
      return syncable::kInvalidMetaHandle;
    }
    return entry.GetMetahandle();
  }

  void ExpectUnsyncedCreation() {
    syncable::ReadTransaction trans(FROM_HERE, directory());
    Entry entry(&trans, GET_BY_CLIENT_TAG, client_tag_);

    EXPECT_EQ(metahandle_, entry.GetMetahandle());
    EXPECT_FALSE(entry.GetIsDel());
    EXPECT_FALSE(entry.GetServerIsDel());  // Never been committed.
    EXPECT_LT(entry.GetBaseVersion(), 0);
    EXPECT_TRUE(entry.GetIsUnsynced());
    EXPECT_FALSE(entry.GetIsUnappliedUpdate());
  }

  void ExpectUnsyncedUndeletion() {
    syncable::ReadTransaction trans(FROM_HERE, directory());
    Entry entry(&trans, GET_BY_CLIENT_TAG, client_tag_);

    EXPECT_EQ(metahandle_, entry.GetMetahandle());
    EXPECT_FALSE(entry.GetIsDel());
    EXPECT_TRUE(entry.GetServerIsDel());
    EXPECT_GE(entry.GetBaseVersion(), 0);
    EXPECT_TRUE(entry.GetIsUnsynced());
    EXPECT_FALSE(entry.GetIsUnappliedUpdate());
    EXPECT_TRUE(entry.GetId().ServerKnows());
  }

  void ExpectUnsyncedEdit() {
    syncable::ReadTransaction trans(FROM_HERE, directory());
    Entry entry(&trans, GET_BY_CLIENT_TAG, client_tag_);

    EXPECT_EQ(metahandle_, entry.GetMetahandle());
    EXPECT_FALSE(entry.GetIsDel());
    EXPECT_FALSE(entry.GetServerIsDel());
    EXPECT_GE(entry.GetBaseVersion(), 0);
    EXPECT_TRUE(entry.GetIsUnsynced());
    EXPECT_FALSE(entry.GetIsUnappliedUpdate());
    EXPECT_TRUE(entry.GetId().ServerKnows());
  }

  void ExpectUnsyncedDeletion() {
    syncable::ReadTransaction trans(FROM_HERE, directory());
    Entry entry(&trans, GET_BY_CLIENT_TAG, client_tag_);

    EXPECT_EQ(metahandle_, entry.GetMetahandle());
    EXPECT_TRUE(entry.GetIsDel());
    EXPECT_FALSE(entry.GetServerIsDel());
    EXPECT_TRUE(entry.GetIsUnsynced());
    EXPECT_FALSE(entry.GetIsUnappliedUpdate());
    EXPECT_GE(entry.GetBaseVersion(), 0);
    EXPECT_GE(entry.GetServerVersion(), 0);
  }

  void ExpectSyncedAndCreated() {
    syncable::ReadTransaction trans(FROM_HERE, directory());
    Entry entry(&trans, GET_BY_CLIENT_TAG, client_tag_);

    EXPECT_EQ(metahandle_, entry.GetMetahandle());
    EXPECT_FALSE(entry.GetIsDel());
    EXPECT_FALSE(entry.GetServerIsDel());
    EXPECT_GE(entry.GetBaseVersion(), 0);
    EXPECT_EQ(entry.GetBaseVersion(), entry.GetServerVersion());
    EXPECT_FALSE(entry.GetIsUnsynced());
    EXPECT_FALSE(entry.GetIsUnappliedUpdate());
  }

  void ExpectSyncedAndDeleted() {
    syncable::ReadTransaction trans(FROM_HERE, directory());
    Entry entry(&trans, GET_BY_CLIENT_TAG, client_tag_);

    EXPECT_EQ(metahandle_, entry.GetMetahandle());
    EXPECT_TRUE(entry.GetIsDel());
    EXPECT_TRUE(entry.GetServerIsDel());
    EXPECT_FALSE(entry.GetIsUnsynced());
    EXPECT_FALSE(entry.GetIsUnappliedUpdate());
    EXPECT_GE(entry.GetBaseVersion(), 0);
    EXPECT_GE(entry.GetServerVersion(), 0);
  }

 protected:
  const std::string client_tag_;
  syncable::Id local_id_;
  int64_t metahandle_;
};

TEST_F(SyncerUndeletionTest, UndeleteDuringCommit) {
  Create();
  ExpectUnsyncedCreation();
  EXPECT_TRUE(SyncShareNudge());

  EXPECT_EQ(1, mock_server_->GetAndClearNumGetUpdatesRequests());
  EXPECT_EQ(0, cycle_->status_controller().TotalNumConflictingItems());
  ExpectSyncedAndCreated();

  // Delete, begin committing the delete, then undelete while committing.
  Delete();
  ExpectUnsyncedDeletion();
  mock_server_->SetMidCommitCallback(
      base::BindOnce(&SyncerUndeletionTest::Undelete, base::Unretained(this)));

  // Commits deletion.
  EXPECT_TRUE(SyncShareNudge());
  sync_pb::SyncEntity deletion_update =
      *mock_server_->AddUpdateFromLastCommit();

  // Commits undeletion.
  mock_server_->SetMidCommitCallback(base::DoNothing());
  EXPECT_TRUE(SyncShareNudge());

  EXPECT_EQ(0, cycle_->status_controller().TotalNumConflictingItems());
  EXPECT_EQ(2, mock_server_->GetAndClearNumGetUpdatesRequests());

  {
    syncable::ReadTransaction trans(FROM_HERE, directory());
    Entry entry(&trans, GET_BY_HANDLE, metahandle_);

    // Server fields lag behind.
    EXPECT_FALSE(entry.GetServerIsDel());

    // We have committed the second (undelete) update.
    EXPECT_FALSE(entry.GetIsDel());
    EXPECT_FALSE(entry.GetIsUnsynced());
    EXPECT_FALSE(entry.GetIsUnappliedUpdate());
  }

  // Now, encounter a GetUpdates corresponding to the deletion from
  // the server.  The undeletion should prevail again and be committed.
  // None of this should trigger any conflict detection -- it is perfectly
  // normal to recieve updates from our own commits.
  deletion_update.set_originator_cache_guid(local_cache_guid());
  deletion_update.set_originator_client_item_id(local_id_.GetServerId());
  *mock_server_->AddUpdateFromLastCommit() = deletion_update;

  EXPECT_TRUE(SyncShareNudge());
  EXPECT_EQ(0, cycle_->status_controller().TotalNumConflictingItems());
  EXPECT_EQ(1, mock_server_->GetAndClearNumGetUpdatesRequests());
  ExpectSyncedAndCreated();
}

TEST_F(SyncerUndeletionTest, UndeleteBeforeCommit) {
  Create();
  ExpectUnsyncedCreation();
  EXPECT_TRUE(SyncShareNudge());

  EXPECT_EQ(1, mock_server_->GetAndClearNumGetUpdatesRequests());
  EXPECT_EQ(0, cycle_->status_controller().TotalNumConflictingItems());
  ExpectSyncedAndCreated();

  // Delete and undelete, then sync to pick up the result.
  Delete();
  ExpectUnsyncedDeletion();
  Undelete();
  ExpectUnsyncedEdit();  // Edit, not undelete: server thinks it exists.
  EXPECT_TRUE(SyncShareNudge());

  // The item ought to have committed successfully.
  EXPECT_EQ(0, cycle_->status_controller().TotalNumConflictingItems());
  EXPECT_EQ(1, mock_server_->GetAndClearNumGetUpdatesRequests());
  ExpectSyncedAndCreated();
  {
    syncable::ReadTransaction trans(FROM_HERE, directory());
    Entry entry(&trans, GET_BY_HANDLE, metahandle_);
    EXPECT_EQ(2, entry.GetBaseVersion());
  }

  // Now, encounter a GetUpdates corresponding to the just-committed
  // update.
  sync_pb::SyncEntity* update = mock_server_->AddUpdateFromLastCommit();
  update->set_originator_cache_guid(local_cache_guid());
  update->set_originator_client_item_id(local_id_.GetServerId());
  EXPECT_TRUE(SyncShareNudge());
  EXPECT_EQ(0, cycle_->status_controller().TotalNumConflictingItems());
  EXPECT_EQ(1, mock_server_->GetAndClearNumGetUpdatesRequests());
  ExpectSyncedAndCreated();
}

TEST_F(SyncerUndeletionTest, UndeleteAfterCommitButBeforeGetUpdates) {
  Create();
  ExpectUnsyncedCreation();
  EXPECT_TRUE(SyncShareNudge());

  EXPECT_EQ(1, mock_server_->GetAndClearNumGetUpdatesRequests());
  EXPECT_EQ(0, cycle_->status_controller().TotalNumConflictingItems());
  ExpectSyncedAndCreated();

  // Delete and commit.
  Delete();
  ExpectUnsyncedDeletion();
  EXPECT_TRUE(SyncShareNudge());

  // The item ought to have committed successfully.
  EXPECT_EQ(0, cycle_->status_controller().TotalNumConflictingItems());
  EXPECT_EQ(1, mock_server_->GetAndClearNumGetUpdatesRequests());
  ExpectSyncedAndDeleted();

  // Before the GetUpdates, the item is locally undeleted.
  Undelete();
  ExpectUnsyncedUndeletion();

  // Now, encounter a GetUpdates corresponding to the just-committed
  // deletion update.  The undeletion should prevail.
  mock_server_->AddUpdateFromLastCommit();
  EXPECT_TRUE(SyncShareNudge());
  EXPECT_EQ(0, cycle_->status_controller().TotalNumConflictingItems());
  EXPECT_EQ(1, mock_server_->GetAndClearNumGetUpdatesRequests());
  ExpectSyncedAndCreated();
}

TEST_F(SyncerUndeletionTest, UndeleteAfterDeleteAndGetUpdates) {
  Create();
  ExpectUnsyncedCreation();
  EXPECT_TRUE(SyncShareNudge());

  EXPECT_EQ(1, mock_server_->GetAndClearNumGetUpdatesRequests());
  EXPECT_EQ(0, cycle_->status_controller().TotalNumConflictingItems());
  ExpectSyncedAndCreated();

  sync_pb::SyncEntity* update = mock_server_->AddUpdateFromLastCommit();
  update->set_originator_cache_guid(local_cache_guid());
  update->set_originator_client_item_id(local_id_.GetServerId());
  EXPECT_TRUE(SyncShareNudge());
  EXPECT_EQ(1, mock_server_->GetAndClearNumGetUpdatesRequests());
  EXPECT_EQ(0, cycle_->status_controller().TotalNumConflictingItems());
  ExpectSyncedAndCreated();

  // Delete and commit.
  Delete();
  ExpectUnsyncedDeletion();
  EXPECT_TRUE(SyncShareNudge());

  // The item ought to have committed successfully.
  EXPECT_EQ(0, cycle_->status_controller().TotalNumConflictingItems());
  EXPECT_EQ(1, mock_server_->GetAndClearNumGetUpdatesRequests());
  ExpectSyncedAndDeleted();

  // Now, encounter a GetUpdates corresponding to the just-committed
  // deletion update.  Should be consistent.
  mock_server_->AddUpdateFromLastCommit();
  EXPECT_TRUE(SyncShareNudge());
  EXPECT_EQ(0, cycle_->status_controller().TotalNumConflictingItems());
  EXPECT_EQ(1, mock_server_->GetAndClearNumGetUpdatesRequests());
  ExpectSyncedAndDeleted();

  // After the GetUpdates, the item is locally undeleted.
  Undelete();
  ExpectUnsyncedUndeletion();

  // Now, encounter a GetUpdates corresponding to the just-committed
  // deletion update.  The undeletion should prevail.
  EXPECT_TRUE(SyncShareNudge());
  EXPECT_EQ(0, cycle_->status_controller().TotalNumConflictingItems());
  EXPECT_EQ(1, mock_server_->GetAndClearNumGetUpdatesRequests());
  ExpectSyncedAndCreated();
}

// Test processing of undeletion GetUpdateses.
TEST_F(SyncerUndeletionTest, UndeleteAfterOtherClientDeletes) {
  Create();
  ExpectUnsyncedCreation();
  EXPECT_TRUE(SyncShareNudge());

  EXPECT_EQ(1, mock_server_->GetAndClearNumGetUpdatesRequests());
  EXPECT_EQ(0, cycle_->status_controller().TotalNumConflictingItems());
  ExpectSyncedAndCreated();

  // Add a delete from the server.
  sync_pb::SyncEntity* update1 = mock_server_->AddUpdateFromLastCommit();
  update1->set_originator_cache_guid(local_cache_guid());
  update1->set_originator_client_item_id(local_id_.GetServerId());
  EXPECT_TRUE(SyncShareNudge());
  EXPECT_EQ(1, mock_server_->GetAndClearNumGetUpdatesRequests());
  EXPECT_EQ(0, cycle_->status_controller().TotalNumConflictingItems());
  ExpectSyncedAndCreated();

  // Some other client deletes the item.
  {
    syncable::ReadTransaction trans(FROM_HERE, directory());
    Entry entry(&trans, GET_BY_HANDLE, metahandle_);
    mock_server_->AddUpdateTombstone(entry.GetId(), PREFERENCES);
  }
  EXPECT_TRUE(SyncShareNudge());

  // The update ought to have applied successfully.
  EXPECT_EQ(0, cycle_->status_controller().TotalNumConflictingItems());
  EXPECT_EQ(1, mock_server_->GetAndClearNumGetUpdatesRequests());
  ExpectSyncedAndDeleted();

  // Undelete it locally.
  Undelete();
  ExpectUnsyncedUndeletion();
  EXPECT_TRUE(SyncShareNudge());
  EXPECT_EQ(0, cycle_->status_controller().TotalNumConflictingItems());
  EXPECT_EQ(1, mock_server_->GetAndClearNumGetUpdatesRequests());
  ExpectSyncedAndCreated();

  // Now, encounter a GetUpdates corresponding to the just-committed
  // deletion update.  The undeletion should prevail.
  sync_pb::SyncEntity* update2 = mock_server_->AddUpdateFromLastCommit();
  update2->set_originator_cache_guid(local_cache_guid());
  update2->set_originator_client_item_id(local_id_.GetServerId());
  EXPECT_TRUE(SyncShareNudge());
  EXPECT_EQ(0, cycle_->status_controller().TotalNumConflictingItems());
  EXPECT_EQ(1, mock_server_->GetAndClearNumGetUpdatesRequests());
  ExpectSyncedAndCreated();
}

TEST_F(SyncerUndeletionTest, UndeleteAfterOtherClientDeletesImmediately) {
  Create();
  ExpectUnsyncedCreation();
  EXPECT_TRUE(SyncShareNudge());

  EXPECT_EQ(1, mock_server_->GetAndClearNumGetUpdatesRequests());
  EXPECT_EQ(0, cycle_->status_controller().TotalNumConflictingItems());
  ExpectSyncedAndCreated();

  // Some other client deletes the item before we get a chance
  // to GetUpdates our original request.
  {
    syncable::ReadTransaction trans(FROM_HERE, directory());
    Entry entry(&trans, GET_BY_HANDLE, metahandle_);
    mock_server_->AddUpdateTombstone(entry.GetId(), PREFERENCES);
  }
  EXPECT_TRUE(SyncShareNudge());

  // The update ought to have applied successfully.
  EXPECT_EQ(0, cycle_->status_controller().TotalNumConflictingItems());
  EXPECT_EQ(1, mock_server_->GetAndClearNumGetUpdatesRequests());
  ExpectSyncedAndDeleted();

  // Undelete it locally.
  Undelete();
  ExpectUnsyncedUndeletion();
  EXPECT_TRUE(SyncShareNudge());
  EXPECT_EQ(0, cycle_->status_controller().TotalNumConflictingItems());
  EXPECT_EQ(1, mock_server_->GetAndClearNumGetUpdatesRequests());
  ExpectSyncedAndCreated();

  // Now, encounter a GetUpdates corresponding to the just-committed
  // deletion update.  The undeletion should prevail.
  sync_pb::SyncEntity* update = mock_server_->AddUpdateFromLastCommit();
  update->set_originator_cache_guid(local_cache_guid());
  update->set_originator_client_item_id(local_id_.GetServerId());
  EXPECT_TRUE(SyncShareNudge());
  EXPECT_EQ(0, cycle_->status_controller().TotalNumConflictingItems());
  EXPECT_EQ(1, mock_server_->GetAndClearNumGetUpdatesRequests());
  ExpectSyncedAndCreated();
}

TEST_F(SyncerUndeletionTest, OtherClientUndeletes) {
  Create();
  ExpectUnsyncedCreation();
  EXPECT_TRUE(SyncShareNudge());

  EXPECT_EQ(1, mock_server_->GetAndClearNumGetUpdatesRequests());
  EXPECT_EQ(0, cycle_->status_controller().TotalNumConflictingItems());
  ExpectSyncedAndCreated();

  // Get the updates of our just-committed entry.
  sync_pb::SyncEntity* update = mock_server_->AddUpdateFromLastCommit();
  update->set_originator_cache_guid(local_cache_guid());
  update->set_originator_client_item_id(local_id_.GetServerId());
  EXPECT_TRUE(SyncShareNudge());
  EXPECT_EQ(1, mock_server_->GetAndClearNumGetUpdatesRequests());
  EXPECT_EQ(0, cycle_->status_controller().TotalNumConflictingItems());
  ExpectSyncedAndCreated();

  // We delete the item.
  Delete();
  ExpectUnsyncedDeletion();
  EXPECT_TRUE(SyncShareNudge());

  // The update ought to have applied successfully.
  EXPECT_EQ(0, cycle_->status_controller().TotalNumConflictingItems());
  EXPECT_EQ(1, mock_server_->GetAndClearNumGetUpdatesRequests());
  ExpectSyncedAndDeleted();

  // Now, encounter a GetUpdates corresponding to the just-committed
  // deletion update.
  mock_server_->AddUpdateFromLastCommit();
  EXPECT_TRUE(SyncShareNudge());
  EXPECT_EQ(0, cycle_->status_controller().TotalNumConflictingItems());
  EXPECT_EQ(1, mock_server_->GetAndClearNumGetUpdatesRequests());
  ExpectSyncedAndDeleted();

  // Some other client undeletes the item.
  {
    syncable::ReadTransaction trans(FROM_HERE, directory());
    Entry entry(&trans, GET_BY_HANDLE, metahandle_);
    mock_server_->AddUpdatePref(entry.GetId().GetServerId(),
                                entry.GetParentId().GetServerId(), client_tag_,
                                100, 1000);
  }
  mock_server_->SetLastUpdateClientTag(client_tag_);
  EXPECT_TRUE(SyncShareNudge());
  EXPECT_EQ(0, cycle_->status_controller().TotalNumConflictingItems());
  EXPECT_EQ(1, mock_server_->GetAndClearNumGetUpdatesRequests());
  ExpectSyncedAndCreated();
}

TEST_F(SyncerUndeletionTest, OtherClientUndeletesImmediately) {
  Create();
  ExpectUnsyncedCreation();
  EXPECT_TRUE(SyncShareNudge());

  EXPECT_EQ(1, mock_server_->GetAndClearNumGetUpdatesRequests());
  EXPECT_EQ(0, cycle_->status_controller().TotalNumConflictingItems());
  ExpectSyncedAndCreated();

  // Get the updates of our just-committed entry.
  sync_pb::SyncEntity* update = mock_server_->AddUpdateFromLastCommit();
  update->set_originator_cache_guid(local_cache_guid());
  {
    syncable::ReadTransaction trans(FROM_HERE, directory());
    Entry entry(&trans, GET_BY_HANDLE, metahandle_);
    update->set_originator_client_item_id(local_id_.GetServerId());
  }
  EXPECT_TRUE(SyncShareNudge());
  EXPECT_EQ(1, mock_server_->GetAndClearNumGetUpdatesRequests());
  EXPECT_EQ(0, cycle_->status_controller().TotalNumConflictingItems());
  ExpectSyncedAndCreated();

  // We delete the item.
  Delete();
  ExpectUnsyncedDeletion();
  EXPECT_TRUE(SyncShareNudge());

  // The update ought to have applied successfully.
  EXPECT_EQ(0, cycle_->status_controller().TotalNumConflictingItems());
  EXPECT_EQ(1, mock_server_->GetAndClearNumGetUpdatesRequests());
  ExpectSyncedAndDeleted();

  // Some other client undeletes before we see the update from our
  // commit.
  {
    syncable::ReadTransaction trans(FROM_HERE, directory());
    Entry entry(&trans, GET_BY_HANDLE, metahandle_);
    mock_server_->AddUpdatePref(entry.GetId().GetServerId(),
                                entry.GetParentId().GetServerId(), client_tag_,
                                100, 1000);
  }
  mock_server_->SetLastUpdateClientTag(client_tag_);
  EXPECT_TRUE(SyncShareNudge());
  EXPECT_EQ(0, cycle_->status_controller().TotalNumConflictingItems());
  EXPECT_EQ(1, mock_server_->GetAndClearNumGetUpdatesRequests());
  ExpectSyncedAndCreated();
}

enum {
  TEST_PARAM_BOOKMARK_ENABLE_BIT,
  TEST_PARAM_AUTOFILL_ENABLE_BIT,
  TEST_PARAM_BIT_COUNT
};

class MixedResult : public SyncerTest,
                    public ::testing::WithParamInterface<int> {
 protected:
  bool ShouldFailBookmarkCommit() {
    return (GetParam() & (1 << TEST_PARAM_BOOKMARK_ENABLE_BIT)) == 0;
  }
  bool ShouldFailAutofillCommit() {
    return (GetParam() & (1 << TEST_PARAM_AUTOFILL_ENABLE_BIT)) == 0;
  }
};

INSTANTIATE_TEST_SUITE_P(ExtensionsActivity,
                         MixedResult,
                         testing::Range(0, 1 << TEST_PARAM_BIT_COUNT));

TEST_P(MixedResult, ExtensionsActivity) {
  {
    syncable::WriteTransaction wtrans(FROM_HERE, UNITTEST, directory());

    MutableEntry pref(&wtrans, CREATE, PREFERENCES, wtrans.root_id(), "pref");
    ASSERT_TRUE(pref.good());
    pref.PutIsUnsynced(true);

    MutableEntry bookmark(&wtrans, CREATE, BOOKMARKS, wtrans.root_id(),
                          "bookmark");
    ASSERT_TRUE(bookmark.good());
    bookmark.PutIsUnsynced(true);

    if (ShouldFailBookmarkCommit()) {
      mock_server_->SetTransientErrorId(bookmark.GetId());
    }

    if (ShouldFailAutofillCommit()) {
      mock_server_->SetTransientErrorId(pref.GetId());
    }
  }

  // Put some extensions activity records into the monitor.
  {
    ExtensionsActivity::Records records;
    records["ABC"].extension_id = "ABC";
    records["ABC"].bookmark_write_count = 2049U;
    records["xyz"].extension_id = "xyz";
    records["xyz"].bookmark_write_count = 4U;
    context_->extensions_activity()->PutRecords(records);
  }

  EXPECT_EQ(!ShouldFailBookmarkCommit() && !ShouldFailAutofillCommit(),
            SyncShareNudge());

  ExtensionsActivity::Records final_monitor_records;
  context_->extensions_activity()->GetAndClearRecords(&final_monitor_records);
  if (ShouldFailBookmarkCommit()) {
    ASSERT_EQ(2U, final_monitor_records.size())
        << "Should restore records after unsuccessful bookmark commit.";
    EXPECT_EQ("ABC", final_monitor_records["ABC"].extension_id);
    EXPECT_EQ("xyz", final_monitor_records["xyz"].extension_id);
    EXPECT_EQ(2049U, final_monitor_records["ABC"].bookmark_write_count);
    EXPECT_EQ(4U, final_monitor_records["xyz"].bookmark_write_count);
  } else {
    EXPECT_TRUE(final_monitor_records.empty())
        << "Should not restore records after successful bookmark commit.";
  }
}

}  // namespace syncer
