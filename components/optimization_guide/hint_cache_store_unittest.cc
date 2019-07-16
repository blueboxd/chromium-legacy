// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/hint_cache_store.h"

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/macros.h"
#include "base/optional.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "components/leveldb_proto/testing/fake_db.h"
#include "components/optimization_guide/hint_update_data.h"
#include "components/optimization_guide/optimization_guide_features.h"
#include "components/optimization_guide/optimization_guide_prefs.h"
#include "components/optimization_guide/proto/hint_cache.pb.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using leveldb_proto::test::FakeDB;
using testing::Mock;

namespace optimization_guide {

namespace {

constexpr char kDefaultComponentVersion[] = "1.0.0";
constexpr char kUpdateComponentVersion[] = "2.0.0";

std::string GetHostSuffix(size_t id) {
  // Host suffix alternates between two different domain types depending on
  // whether the id is odd or even.
  if (id % 2 == 0) {
    return "domain" + base::NumberToString(id) + ".org";
  } else {
    return "different.domain" + base::NumberToString(id) + ".co.in";
  }
}

enum class MetadataSchemaState {
  kMissing,
  kInvalid,
  kValid,
};

}  // namespace

class HintCacheStoreTest : public testing::Test {
 public:
  using StoreEntry = proto::StoreEntry;
  using StoreEntryMap = std::map<HintCacheStore::EntryKey, StoreEntry>;

  HintCacheStoreTest() : db_(nullptr) {}

  void TearDown() override { last_loaded_hint_.reset(); }

  // Initializes the entries contained within the database on startup.
  void SeedInitialData(
      MetadataSchemaState state,
      base::Optional<size_t> component_hint_count = base::Optional<size_t>(),
      base::Optional<base::Time> fetched_hints_update =
          base::Optional<base::Time>()) {
    db_store_.clear();

    // Add a metadata schema entry if its state isn't kMissing. The version
    // entry version is set to the store's current version if the state is
    // kValid; otherwise, it's set to the invalid version of "0".
    if (state == MetadataSchemaState::kValid) {
      db_store_[HintCacheStore::GetMetadataTypeEntryKey(
                    HintCacheStore::MetadataType::kSchema)]
          .set_version(HintCacheStore::kStoreSchemaVersion);
    } else if (state == MetadataSchemaState::kInvalid) {
      db_store_[HintCacheStore::GetMetadataTypeEntryKey(
                    HintCacheStore::MetadataType::kSchema)]
          .set_version("0");
    }

    // If the database is being seeded with component hints, it is indicated
    // with a provided count. Add the component metadata with the default
    // component version and then add the indicated number of component hints.
    // if (component_hint_count && component_hint_count >
    // static_cast<size_t>(0)) {
    if (component_hint_count && component_hint_count > 0u) {
      db_store_[HintCacheStore::GetMetadataTypeEntryKey(
                    HintCacheStore::MetadataType::kComponent)]
          .set_version(kDefaultComponentVersion);
      HintCacheStore::EntryKeyPrefix component_hint_key_prefix =
          HintCacheStore::GetComponentHintEntryKeyPrefix(
              base::Version(kDefaultComponentVersion));
      for (size_t i = 0; i < component_hint_count.value(); ++i) {
        std::string host_suffix = GetHostSuffix(i);
        StoreEntry& entry = db_store_[component_hint_key_prefix + host_suffix];
        entry.set_entry_type(static_cast<proto::StoreEntryType>(
            HintCacheStore::StoreEntryType::kComponentHint));
        proto::Hint* hint = entry.mutable_hint();
        hint->set_key(host_suffix);
        hint->set_key_representation(proto::HOST_SUFFIX);
        proto::PageHint* page_hint = hint->add_page_hints();
        page_hint->set_page_pattern("page pattern " + base::NumberToString(i));
      }
    }
    if (fetched_hints_update) {
      db_store_[HintCacheStore::GetMetadataTypeEntryKey(
                    HintCacheStore::MetadataType::kFetched)]
          .set_update_time_secs(
              fetched_hints_update->ToDeltaSinceWindowsEpoch().InSeconds());
    }
  }

  // Moves the specified number of component hints into the update data.
  void SeedComponentUpdateData(HintUpdateData* update_data,
                               size_t component_hint_count) {
    for (size_t i = 0; i < component_hint_count; ++i) {
      std::string host_suffix = GetHostSuffix(i);
      proto::Hint hint;
      hint.set_key(host_suffix);
      hint.set_key_representation(proto::HOST_SUFFIX);
      proto::PageHint* page_hint = hint.add_page_hints();
      page_hint->set_page_pattern("page pattern " + base::NumberToString(i));
      update_data->MoveHintIntoUpdateData(std::move(hint));
    }
  }
  // Moves the specified number of component hints into the update data.
  void SeedFetchedUpdateData(HintUpdateData* update_data,
                             size_t fetched_hint_count) {
    for (size_t i = 0; i < fetched_hint_count; ++i) {
      std::string host_suffix = GetHostSuffix(i);
      proto::Hint hint;
      hint.set_key(host_suffix);
      hint.set_key_representation(proto::HOST_SUFFIX);
      proto::PageHint* page_hint = hint.add_page_hints();
      page_hint->set_page_pattern("page pattern " + base::NumberToString(i));
      update_data->MoveHintIntoUpdateData(std::move(hint));
    }
  }

  void CreateDatabase() {
    // Reset everything.
    db_ = nullptr;
    hint_store_.reset();

    // Setup the fake db and the class under test.
    auto db = std::make_unique<FakeDB<StoreEntry>>(&db_store_);
    db_ = db.get();

    optimization_guide::prefs::RegisterProfilePrefs(pref_service_.registry());
    hint_store_ =
        std::make_unique<HintCacheStore>(std::move(db), &pref_service_);
  }

  void InitializeDatabase(bool success, bool purge_existing_data = false) {
    EXPECT_CALL(*this, OnInitialized());
    hint_store()->Initialize(purge_existing_data,
                             base::BindOnce(&HintCacheStoreTest::OnInitialized,
                                            base::Unretained(this)));
    // OnDatabaseInitialized callback
    db()->InitStatusCallback(success ? leveldb_proto::Enums::kOK
                                     : leveldb_proto::Enums::kError);
  }

  void InitializeStore(MetadataSchemaState state,
                       bool purge_existing_data = false) {
    InitializeDatabase(true /*=success*/, purge_existing_data);

    if (purge_existing_data) {
      // OnPurgeDatabase callback
      db()->UpdateCallback(true);
      return;
    }

    // OnLoadMetadata callback
    db()->LoadCallback(true);
    if (state == MetadataSchemaState::kValid) {
      // OnLoadHintEntryKeys callback
      db()->LoadCallback(true);
    } else {
      // OnPurgeDatabase callback
      db()->UpdateCallback(true);
    }
  }

  void UpdateComponentHints(std::unique_ptr<HintUpdateData> component_data,
                            bool update_success = true,
                            bool load_hint_entry_keys_success = true) {
    EXPECT_CALL(*this, OnUpdateHints());
    hint_store()->UpdateComponentHints(
        std::move(component_data),
        base::BindOnce(&HintCacheStoreTest::OnUpdateHints,
                       base::Unretained(this)));
    // OnUpdateHints callback
    db()->UpdateCallback(update_success);
    if (update_success) {
      // OnLoadHintEntryKeys callback
      db()->LoadCallback(load_hint_entry_keys_success);
    }
  }

  void UpdateFetchedHints(std::unique_ptr<HintUpdateData> fetched_data,
                          bool update_success = true,
                          bool load_hint_entry_keys_success = true) {
    EXPECT_CALL(*this, OnUpdateHints());
    hint_store()->UpdateFetchedHints(
        std::move(fetched_data),
        base::BindOnce(&HintCacheStoreTest::OnUpdateHints,
                       base::Unretained(this)));
    // OnUpdateHints callback
    db()->UpdateCallback(update_success);
    if (update_success) {
      // OnLoadHintEntryKeys callback
      db()->LoadCallback(load_hint_entry_keys_success);
    }
  }

  void ClearFetchedHintsFromDatabase() {
    hint_store()->ClearFetchedHintsFromDatabase();
    db()->UpdateCallback(true);
    db()->LoadCallback(true);
  }

  void PurgeExpiredFetchedHints() {
    hint_store()->PurgeExpiredFetchedHints();

    // OnFetchedHintsLoadedToMaybePurge
    db()->LoadCallback(true);
    // OnUpdateHints
    db()->UpdateCallback(true);
    // OnLoadHintEntryKeys callback
    db()->LoadCallback(true);
  }

  bool IsMetadataSchemaEntryKeyPresent() const {
    return IsKeyPresent(HintCacheStore::GetMetadataTypeEntryKey(
        HintCacheStore::MetadataType::kSchema));
  }

  // Verifies that the fetched metadata has the expected next update time.
  void ExpectFetchedMetadata(base::Time update_time) const {
    const auto& metadata_entry =
        db_store_.find(HintCacheStore::GetMetadataTypeEntryKey(
            HintCacheStore::MetadataType::kFetched));
    if (metadata_entry != db_store_.end()) {
      // The next update time should have same time up to the second as the
      // metadata entry is stored in seconds.
      EXPECT_TRUE(
          base::Time::FromDeltaSinceWindowsEpoch(base::TimeDelta::FromSeconds(
              metadata_entry->second.update_time_secs())) -
              update_time <
          base::TimeDelta::FromSeconds(1));
    } else {
      FAIL() << "No fetched metadata found";
    }
  }
  // Verifies that the component metadata has the expected version and all
  // expected component hints are present.
  void ExpectComponentHintsPresent(const std::string& version,
                                   int count) const {
    const auto& metadata_entry =
        db_store_.find(HintCacheStore::GetMetadataTypeEntryKey(
            HintCacheStore::MetadataType::kComponent));
    if (metadata_entry != db_store_.end()) {
      EXPECT_EQ(metadata_entry->second.version(), version);
    } else {
      FAIL() << "No component metadata found";
    }

    HintCacheStore::EntryKeyPrefix component_hint_entry_key_prefix =
        HintCacheStore::GetComponentHintEntryKeyPrefix(base::Version(version));
    for (int i = 0; i < count; ++i) {
      std::string host_suffix = GetHostSuffix(i);
      HintCacheStore::EntryKey hint_entry_key =
          component_hint_entry_key_prefix + host_suffix;
      const auto& hint_entry = db_store_.find(hint_entry_key);
      if (hint_entry == db_store_.end()) {
        FAIL() << "No entry found for component hint: " << hint_entry_key;
        continue;
      }

      if (!hint_entry->second.has_hint()) {
        FAIL() << "Component hint entry does not have hint: " << hint_entry_key;
        continue;
      }

      EXPECT_EQ(hint_entry->second.hint().key(), host_suffix);
    }
  }

  // Returns true if the data is present for the given key.
  bool IsKeyPresent(const HintCacheStore::EntryKey& entry_key) const {
    return db_store_.find(entry_key) != db_store_.end();
  }

  size_t GetDBStoreEntryCount() const { return db_store_.size(); }
  size_t GetStoreHintEntryKeyCount() const {
    return hint_store_->GetHintEntryKeyCount();
  }

  HintCacheStore* hint_store() { return hint_store_.get(); }
  FakeDB<proto::StoreEntry>* db() { return db_; }

  const HintCacheStore::EntryKey& last_loaded_hint_entry_key() const {
    return last_loaded_hint_entry_key_;
  }

  proto::Hint* last_loaded_hint() { return last_loaded_hint_.get(); }

  void OnHintLoaded(const HintCacheStore::EntryKey& hint_entry_key,
                    std::unique_ptr<proto::Hint> loaded_hint) {
    last_loaded_hint_entry_key_ = hint_entry_key;
    last_loaded_hint_ = std::move(loaded_hint);
  }

  const base::DictionaryValue* hint_loaded_counts_pref() {
    return pref_service_.GetDictionary(
        optimization_guide::prefs::kHintLoadedCounts);
  }

  MOCK_METHOD0(OnInitialized, void());
  MOCK_METHOD0(OnUpdateHints, void());

 private:
  FakeDB<proto::StoreEntry>* db_;
  StoreEntryMap db_store_;
  TestingPrefServiceSimple pref_service_;
  std::unique_ptr<HintCacheStore> hint_store_;

  HintCacheStore::EntryKey last_loaded_hint_entry_key_;
  std::unique_ptr<proto::Hint> last_loaded_hint_;

  DISALLOW_COPY_AND_ASSIGN(HintCacheStoreTest);
};

TEST_F(HintCacheStoreTest, NoInitialization) {
  base::HistogramTester histogram_tester;

  SeedInitialData(MetadataSchemaState::kMissing);
  CreateDatabase();

  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.HintCacheLevelDBStore.LoadMetadataResult", 0);

  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.HintCacheLevelDBStore.Status", 0 /* kUninitialized */,
      1);
  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.HintCacheLevelDBStore.Status", 1 /* kInitializing */,
      0);
  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.HintCacheLevelDBStore.Status", 2 /* kAvailable */, 0);
  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.HintCacheLevelDBStore.Status", 3 /* kFailed */, 0);
}

TEST_F(HintCacheStoreTest, InitializeFailedOnInitializeWithNoInitialData) {
  base::HistogramTester histogram_tester;

  SeedInitialData(MetadataSchemaState::kMissing);
  CreateDatabase();
  InitializeDatabase(false /*=success*/);

  // In the case where initialization fails, the store should be fully purged.
  EXPECT_EQ(GetDBStoreEntryCount(), static_cast<size_t>(0));
  EXPECT_EQ(GetStoreHintEntryKeyCount(), static_cast<size_t>(0));

  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.HintCacheLevelDBStore.LoadMetadataResult", 0);

  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.HintCacheLevelDBStore.Status", 0 /* kUninitialized */,
      1);
  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.HintCacheLevelDBStore.Status", 1 /* kInitializing */,
      1);
  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.HintCacheLevelDBStore.Status", 2 /* kAvailable */, 0);
  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.HintCacheLevelDBStore.Status", 3 /* kFailed */, 1);
}

TEST_F(HintCacheStoreTest, InitializeFailedOnLoadMetadataWithNoInitialData) {
  base::HistogramTester histogram_tester;

  SeedInitialData(MetadataSchemaState::kMissing);
  CreateDatabase();
  InitializeDatabase(true /*=success*/);

  // OnLoadMetadata callback
  db()->LoadCallback(false);

  // In the case where initialization fails, the store should be fully purged.
  EXPECT_EQ(GetDBStoreEntryCount(), static_cast<size_t>(0));
  EXPECT_EQ(GetStoreHintEntryKeyCount(), static_cast<size_t>(0));

  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.HintCacheLevelDBStore.LoadMetadataResult",
      1 /* kLoadMetadataFailed */, 1);

  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.HintCacheLevelDBStore.Status", 0 /* kUninitialized */,
      1);
  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.HintCacheLevelDBStore.Status", 1 /* kInitializing */,
      1);
  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.HintCacheLevelDBStore.Status", 2 /* kAvailable */, 0);
  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.HintCacheLevelDBStore.Status", 3 /* kFailed */, 1);
}

TEST_F(HintCacheStoreTest, InitializeFailedOnUpdateMetadataNoInitialData) {
  base::HistogramTester histogram_tester;

  SeedInitialData(MetadataSchemaState::kMissing);
  CreateDatabase();

  InitializeDatabase(true /*=success*/);

  // OnLoadMetadata callback
  db()->LoadCallback(true);
  // OnPurgeDatabase callback
  db()->UpdateCallback(false);

  // In the case where initialization fails, the store should be fully purged.
  EXPECT_EQ(GetDBStoreEntryCount(), static_cast<size_t>(0));
  EXPECT_EQ(GetStoreHintEntryKeyCount(), static_cast<size_t>(0));

  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.HintCacheLevelDBStore.LoadMetadataResult",
      2 /* kSchemaMetadataMissing */, 1);

  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.HintCacheLevelDBStore.Status", 0 /* kUninitialized */,
      1);
  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.HintCacheLevelDBStore.Status", 1 /* kInitializing */,
      1);
  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.HintCacheLevelDBStore.Status", 2 /* kAvailable */, 0);
  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.HintCacheLevelDBStore.Status", 3 /* kFailed */, 1);
}

TEST_F(HintCacheStoreTest, InitializeFailedOnInitializeWithInitialData) {
  base::HistogramTester histogram_tester;

  SeedInitialData(MetadataSchemaState::kValid, 10);
  CreateDatabase();
  InitializeDatabase(false /*=success*/);

  // In the case where initialization fails, the store should be fully purged.
  EXPECT_EQ(GetDBStoreEntryCount(), static_cast<size_t>(0));
  EXPECT_EQ(GetStoreHintEntryKeyCount(), static_cast<size_t>(0));

  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.HintCacheLevelDBStore.LoadMetadataResult", 0);

  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.HintCacheLevelDBStore.Status", 0 /* kUninitialized */,
      1);
  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.HintCacheLevelDBStore.Status", 1 /* kInitializing */,
      1);
  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.HintCacheLevelDBStore.Status", 2 /* kAvailable */, 0);
  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.HintCacheLevelDBStore.Status", 3 /* kFailed */, 1);
}

TEST_F(HintCacheStoreTest, InitializeFailedOnLoadMetadataWithInitialData) {
  base::HistogramTester histogram_tester;

  SeedInitialData(MetadataSchemaState::kValid, 10);
  CreateDatabase();
  InitializeDatabase(true /*=success*/);

  // OnLoadMetadata callback
  db()->LoadCallback(false);

  // In the case where initialization fails, the store should be fully purged.
  EXPECT_EQ(GetDBStoreEntryCount(), static_cast<size_t>(0));
  EXPECT_EQ(GetStoreHintEntryKeyCount(), static_cast<size_t>(0));

  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.HintCacheLevelDBStore.LoadMetadataResult",
      1 /* kLoadMetadataFailed */, 1);

  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.HintCacheLevelDBStore.Status", 0 /* kUninitialized */,
      1);
  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.HintCacheLevelDBStore.Status", 1 /* kInitializing */,
      1);
  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.HintCacheLevelDBStore.Status", 2 /* kAvailable */, 0);
  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.HintCacheLevelDBStore.Status", 3 /* kFailed */, 1);
}

TEST_F(HintCacheStoreTest,
       InitializeFailedOnUpdateMetadataWithInvalidSchemaEntry) {
  base::HistogramTester histogram_tester;

  SeedInitialData(MetadataSchemaState::kInvalid, 10);
  CreateDatabase();
  InitializeDatabase(true /*=success*/);

  // OnLoadMetadata callback
  db()->LoadCallback(true);
  // OnPurgeDatabase callback
  db()->UpdateCallback(false);

  // In the case where initialization fails, the store should be fully purged.
  EXPECT_EQ(GetDBStoreEntryCount(), static_cast<size_t>(0));
  EXPECT_EQ(GetStoreHintEntryKeyCount(), static_cast<size_t>(0));

  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.HintCacheLevelDBStore.LoadMetadataResult",
      3 /* kSchemaMetadataWrongVersion */, 1);

  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.HintCacheLevelDBStore.Status", 0 /* kUninitialized */,
      1);
  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.HintCacheLevelDBStore.Status", 1 /* kInitializing */,
      1);
  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.HintCacheLevelDBStore.Status", 2 /* kAvailable */, 0);
  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.HintCacheLevelDBStore.Status", 3 /* kFailed */, 1);
}

TEST_F(HintCacheStoreTest, InitializeFailedOnLoadHintEntryKeysWithInitialData) {
  base::HistogramTester histogram_tester;

  SeedInitialData(MetadataSchemaState::kValid, 10, base::Time().Now());
  CreateDatabase();
  InitializeDatabase(true /*=success*/);

  // OnLoadMetadata callback
  db()->LoadCallback(true);
  // OnLoadHintEntryKeys callback
  db()->LoadCallback(false);

  // In the case where initialization fails, the store should be fully purged.
  EXPECT_EQ(GetDBStoreEntryCount(), static_cast<size_t>(0));
  EXPECT_EQ(GetStoreHintEntryKeyCount(), static_cast<size_t>(0));

  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.HintCacheLevelDBStore.LoadMetadataResult",
      0 /* kSuccess */, 1);

  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.HintCacheLevelDBStore.Status", 0 /* kUninitialized */,
      1);
  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.HintCacheLevelDBStore.Status", 1 /* kInitializing */,
      1);
  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.HintCacheLevelDBStore.Status", 2 /* kAvailable */, 1);
  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.HintCacheLevelDBStore.Status", 3 /* kFailed */, 1);
}

TEST_F(HintCacheStoreTest, InitializeSucceededWithoutSchemaEntry) {
  base::HistogramTester histogram_tester;

  MetadataSchemaState schema_state = MetadataSchemaState::kMissing;
  SeedInitialData(schema_state);
  CreateDatabase();
  InitializeStore(schema_state);

  // The store should contain the schema metadata entry and nothing else.
  EXPECT_EQ(GetDBStoreEntryCount(), static_cast<size_t>(1));
  EXPECT_EQ(GetStoreHintEntryKeyCount(), static_cast<size_t>(0));

  EXPECT_TRUE(IsMetadataSchemaEntryKeyPresent());

  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.HintCacheLevelDBStore.LoadMetadataResult",
      2 /* kSchemaMetadataMissing */, 1);

  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.HintCacheLevelDBStore.Status", 0 /* kUninitialized */,
      1);
  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.HintCacheLevelDBStore.Status", 1 /* kInitializing */,
      1);
  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.HintCacheLevelDBStore.Status", 2 /* kAvailable */, 1);
  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.HintCacheLevelDBStore.Status", 3 /* kFailed */, 0);
}

TEST_F(HintCacheStoreTest, InitializeSucceededWithInvalidSchemaEntry) {
  base::HistogramTester histogram_tester;

  MetadataSchemaState schema_state = MetadataSchemaState::kInvalid;
  SeedInitialData(schema_state);
  CreateDatabase();
  InitializeStore(schema_state);

  // The store should contain the schema metadata entry and nothing else.
  EXPECT_EQ(GetDBStoreEntryCount(), static_cast<size_t>(1));
  EXPECT_EQ(GetStoreHintEntryKeyCount(), static_cast<size_t>(0));

  EXPECT_TRUE(IsMetadataSchemaEntryKeyPresent());

  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.HintCacheLevelDBStore.LoadMetadataResult",
      3 /* kSchemaMetadataWrongVersion */, 1);

  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.HintCacheLevelDBStore.Status", 0 /* kUninitialized */,
      1);
  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.HintCacheLevelDBStore.Status", 1 /* kInitializing */,
      1);
  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.HintCacheLevelDBStore.Status", 2 /* kAvailable */, 1);
  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.HintCacheLevelDBStore.Status", 3 /* kFailed */, 0);
}

TEST_F(HintCacheStoreTest, InitializeSucceededWithValidSchemaEntry) {
  base::HistogramTester histogram_tester;

  MetadataSchemaState schema_state = MetadataSchemaState::kValid;
  SeedInitialData(schema_state);
  CreateDatabase();
  InitializeStore(schema_state);

  // The store should contain the schema metadata entry and nothing else.
  EXPECT_EQ(GetDBStoreEntryCount(), static_cast<size_t>(1));
  EXPECT_EQ(GetStoreHintEntryKeyCount(), static_cast<size_t>(0));

  EXPECT_TRUE(IsMetadataSchemaEntryKeyPresent());

  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.HintCacheLevelDBStore.LoadMetadataResult",
      4 /* kComponentMetadataMissing*/, 0);
  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.HintCacheLevelDBStore.LoadMetadataResult",
      5 /* kFetchedMetadataMissing*/, 0);
  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.HintCacheLevelDBStore.LoadMetadataResult",
      6 /* kComponentAndFetchedMetadataMissing*/, 1);

  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.HintCacheLevelDBStore.Status", 0 /* kUninitialized */,
      1);
  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.HintCacheLevelDBStore.Status", 1 /* kInitializing */,
      1);
  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.HintCacheLevelDBStore.Status", 2 /* kAvailable */, 1);
  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.HintCacheLevelDBStore.Status", 3 /* kFailed */, 0);
}

TEST_F(HintCacheStoreTest,
       InitializeSucceededWithInvalidSchemaEntryAndInitialData) {
  base::HistogramTester histogram_tester;

  MetadataSchemaState schema_state = MetadataSchemaState::kInvalid;
  SeedInitialData(schema_state, 10);
  CreateDatabase();
  InitializeStore(schema_state);

  // The store should contain the schema metadata entry and nothing else, as
  // the initial component hints are all purged.
  EXPECT_EQ(GetDBStoreEntryCount(), static_cast<size_t>(1));
  EXPECT_EQ(GetStoreHintEntryKeyCount(), static_cast<size_t>(0));

  EXPECT_TRUE(IsMetadataSchemaEntryKeyPresent());

  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.HintCacheLevelDBStore.LoadMetadataResult",
      3 /* kSchemaMetadataWrongVersion */, 1);

  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.HintCacheLevelDBStore.Status", 0 /* kUninitialized */,
      1);
  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.HintCacheLevelDBStore.Status", 1 /* kInitializing */,
      1);
  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.HintCacheLevelDBStore.Status", 2 /* kAvailable */, 1);
  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.HintCacheLevelDBStore.Status", 3 /* kFailed */, 0);
}

TEST_F(HintCacheStoreTest, InitializeSucceededWithPurgeExistingData) {
  base::HistogramTester histogram_tester;

  MetadataSchemaState schema_state = MetadataSchemaState::kValid;
  SeedInitialData(schema_state, 10);
  CreateDatabase();
  InitializeStore(schema_state, true /*=purge_existing_data*/);

  // The store should contain the schema metadata entry and nothing else.
  EXPECT_EQ(GetDBStoreEntryCount(), static_cast<size_t>(1));
  EXPECT_EQ(GetStoreHintEntryKeyCount(), static_cast<size_t>(0));

  EXPECT_TRUE(IsMetadataSchemaEntryKeyPresent());

  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.HintCacheLevelDBStore.LoadMetadataResult", 0);

  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.HintCacheLevelDBStore.Status", 0 /* kUninitialized */,
      1);
  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.HintCacheLevelDBStore.Status", 1 /* kInitializing */,
      1);
  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.HintCacheLevelDBStore.Status", 2 /* kAvailable */, 1);
  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.HintCacheLevelDBStore.Status", 3 /* kFailed */, 0);
}

TEST_F(HintCacheStoreTest,
       InitializeSucceededWithValidSchemaEntryAndInitialData) {
  base::HistogramTester histogram_tester;

  MetadataSchemaState schema_state = MetadataSchemaState::kValid;
  size_t component_hint_count = 10;
  SeedInitialData(schema_state, component_hint_count, base::Time().Now());
  CreateDatabase();
  InitializeStore(schema_state);

  // The store should contain the schema metadata entry, the component metadata
  // entry, and all of the initial component hints.
  EXPECT_EQ(GetDBStoreEntryCount(),
            static_cast<size_t>(component_hint_count + 3));
  EXPECT_EQ(GetStoreHintEntryKeyCount(), component_hint_count);

  EXPECT_TRUE(IsMetadataSchemaEntryKeyPresent());
  ExpectComponentHintsPresent(kDefaultComponentVersion, component_hint_count);

  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.HintCacheLevelDBStore.LoadMetadataResult",
      0 /* kSuccess */, 1);

  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.HintCacheLevelDBStore.Status", 0 /* kUninitialized */,
      1);
  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.HintCacheLevelDBStore.Status", 1 /* kInitializing */,
      1);
  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.HintCacheLevelDBStore.Status", 2 /* kAvailable */, 1);
  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.HintCacheLevelDBStore.Status", 3 /* kFailed */, 0);
}

TEST_F(HintCacheStoreTest,
       InitializeSucceededWithValidSchemaEntryAndComponentDataOnly) {
  base::HistogramTester histogram_tester;

  MetadataSchemaState schema_state = MetadataSchemaState::kValid;
  size_t component_hint_count = 10;
  SeedInitialData(schema_state, component_hint_count);
  CreateDatabase();
  InitializeStore(schema_state);

  // The store should contain the schema metadata entry, the component metadata
  // entry, and all of the initial component hints.
  EXPECT_EQ(GetDBStoreEntryCount(),
            static_cast<size_t>(component_hint_count + 2));
  EXPECT_EQ(GetStoreHintEntryKeyCount(), component_hint_count);

  EXPECT_TRUE(IsMetadataSchemaEntryKeyPresent());
  ExpectComponentHintsPresent(kDefaultComponentVersion, component_hint_count);

  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.HintCacheLevelDBStore.LoadMetadataResult",
      4 /* kComponentMetadataMissing*/, 0);
  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.HintCacheLevelDBStore.LoadMetadataResult",
      5 /* kFetchedMetadataMissing*/, 1);
  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.HintCacheLevelDBStore.LoadMetadataResult",
      6 /* kComponentAndFetchedMetadataMissing*/, 0);

  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.HintCacheLevelDBStore.Status", 0 /* kUninitialized */,
      1);
  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.HintCacheLevelDBStore.Status", 1 /* kInitializing */,
      1);
  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.HintCacheLevelDBStore.Status", 2 /* kAvailable */, 1);
  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.HintCacheLevelDBStore.Status", 3 /* kFailed */, 0);
}

TEST_F(HintCacheStoreTest,
       InitializeSucceededWithValidSchemaEntryAndFetchedMetaData) {
  base::HistogramTester histogram_tester;

  MetadataSchemaState schema_state = MetadataSchemaState::kValid;
  size_t component_hint_count = 0;
  SeedInitialData(schema_state, component_hint_count, base::Time().Now());
  CreateDatabase();
  InitializeStore(schema_state);

  // The store should contain the schema metadata entry, the component metadata
  // entry, and all of the initial component hints.
  EXPECT_EQ(GetDBStoreEntryCount(),
            static_cast<size_t>(component_hint_count + 2));
  EXPECT_EQ(GetStoreHintEntryKeyCount(), component_hint_count);

  EXPECT_TRUE(IsMetadataSchemaEntryKeyPresent());

  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.HintCacheLevelDBStore.LoadMetadataResult",
      4 /* kComponentMetadataMissing*/, 1);

  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.HintCacheLevelDBStore.Status", 0 /* kUninitialized */,
      1);
  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.HintCacheLevelDBStore.Status", 1 /* kInitializing */,
      1);
  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.HintCacheLevelDBStore.Status", 2 /* kAvailable */, 1);
  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.HintCacheLevelDBStore.Status", 3 /* kFailed */, 0);
}

TEST_F(HintCacheStoreTest,
       CreateComponentUpdateDataFailsForUninitializedStore) {
  MetadataSchemaState schema_state = MetadataSchemaState::kValid;
  SeedInitialData(schema_state, 10);
  CreateDatabase();

  // HintUpdateData for a component update should only be created if the store
  // is initialized.
  EXPECT_FALSE(hint_store()->MaybeCreateUpdateDataForComponentHints(
      base::Version(kUpdateComponentVersion)));
}

TEST_F(HintCacheStoreTest, CreateComponentUpdateDataFailsForEarlierVersion) {
  MetadataSchemaState schema_state = MetadataSchemaState::kValid;
  SeedInitialData(schema_state, 10);
  CreateDatabase();
  InitializeStore(schema_state);

  // No HintUpdateData for a component update should be created when the
  // component version of the update is older than the store's component
  // version.
  EXPECT_FALSE(hint_store()->MaybeCreateUpdateDataForComponentHints(
      base::Version("0.0.0")));
}

TEST_F(HintCacheStoreTest, CreateComponentUpdateDataFailsForCurrentVersion) {
  MetadataSchemaState schema_state = MetadataSchemaState::kValid;
  SeedInitialData(schema_state, 10);
  CreateDatabase();
  InitializeStore(schema_state);

  // No HintUpdateData should be created when the component version of the
  // update is the same as the store's component version.
  EXPECT_FALSE(hint_store()->MaybeCreateUpdateDataForComponentHints(
      base::Version(kDefaultComponentVersion)));
}

TEST_F(HintCacheStoreTest,
       CreateComponentUpdateDataSucceedsWithNoPreexistingVersion) {
  MetadataSchemaState schema_state = MetadataSchemaState::kValid;
  SeedInitialData(schema_state);
  CreateDatabase();
  InitializeStore(schema_state);

  // HintUpdateData for a component update should be created when there is no
  // pre-existing component in the store.
  EXPECT_TRUE(hint_store()->MaybeCreateUpdateDataForComponentHints(
      base::Version(kDefaultComponentVersion)));
}

TEST_F(HintCacheStoreTest, CreateComponentUpdateDataSucceedsForNewerVersion) {
  MetadataSchemaState schema_state = MetadataSchemaState::kValid;
  SeedInitialData(schema_state, 10);
  CreateDatabase();
  InitializeStore(schema_state);

  // HintUpdateData for a component update should be created when the component
  // version of the update is newer than the store's component version.
  EXPECT_TRUE(hint_store()->MaybeCreateUpdateDataForComponentHints(
      base::Version(kUpdateComponentVersion)));
}

TEST_F(HintCacheStoreTest, UpdateComponentHintsUpdateEntriesFails) {
  MetadataSchemaState schema_state = MetadataSchemaState::kValid;
  SeedInitialData(schema_state, 10);
  CreateDatabase();
  InitializeStore(schema_state);

  std::unique_ptr<HintUpdateData> update_data =
      hint_store()->MaybeCreateUpdateDataForComponentHints(
          base::Version(kUpdateComponentVersion));
  ASSERT_TRUE(update_data);
  SeedComponentUpdateData(update_data.get(), 5);

  UpdateComponentHints(std::move(update_data), false /*update_success*/);

  // The store should be purged if the component data update fails.
  EXPECT_EQ(GetDBStoreEntryCount(), static_cast<size_t>(0));
  EXPECT_EQ(GetStoreHintEntryKeyCount(), static_cast<size_t>(0));
}

TEST_F(HintCacheStoreTest, UpdateComponentHintsGetKeysFails) {
  MetadataSchemaState schema_state = MetadataSchemaState::kValid;
  SeedInitialData(schema_state, 10);
  CreateDatabase();
  InitializeStore(schema_state);

  std::unique_ptr<HintUpdateData> update_data =
      hint_store()->MaybeCreateUpdateDataForComponentHints(
          base::Version(kUpdateComponentVersion));
  ASSERT_TRUE(update_data);
  SeedComponentUpdateData(update_data.get(), 5);

  UpdateComponentHints(std::move(update_data), true /*update_success*/,
                       false /*load_hints_keys_success*/);

  // The store should be purged if loading the keys after the component update
  // fails.
  EXPECT_EQ(GetDBStoreEntryCount(), static_cast<size_t>(0));
  EXPECT_EQ(GetStoreHintEntryKeyCount(), static_cast<size_t>(0));
}

TEST_F(HintCacheStoreTest, UpdateComponentHints) {
  MetadataSchemaState schema_state = MetadataSchemaState::kValid;
  size_t initial_hint_count = 10;
  size_t update_hint_count = 5;
  SeedInitialData(schema_state, initial_hint_count);
  CreateDatabase();
  InitializeStore(schema_state);

  std::unique_ptr<HintUpdateData> update_data =
      hint_store()->MaybeCreateUpdateDataForComponentHints(
          base::Version(kUpdateComponentVersion));
  ASSERT_TRUE(update_data);
  SeedComponentUpdateData(update_data.get(), update_hint_count);
  UpdateComponentHints(std::move(update_data));

  // When the component update succeeds, the store should contain the schema
  // metadata entry, the component metadata entry, and all of the update's
  // component hints.
  EXPECT_EQ(GetDBStoreEntryCount(), update_hint_count + 2);
  EXPECT_EQ(GetStoreHintEntryKeyCount(), update_hint_count);
  ExpectComponentHintsPresent(kUpdateComponentVersion, update_hint_count);
}

TEST_F(HintCacheStoreTest, UpdateComponentHintsAfterInitializationDataPurge) {
  MetadataSchemaState schema_state = MetadataSchemaState::kValid;
  size_t initial_hint_count = 10;
  size_t update_hint_count = 5;
  SeedInitialData(schema_state, initial_hint_count);
  CreateDatabase();
  InitializeStore(schema_state, true /*=purge_existing_data*/);

  std::unique_ptr<HintUpdateData> update_data =
      hint_store()->MaybeCreateUpdateDataForComponentHints(
          base::Version(kUpdateComponentVersion));
  ASSERT_TRUE(update_data);
  SeedComponentUpdateData(update_data.get(), update_hint_count);
  UpdateComponentHints(std::move(update_data));

  // When the component update succeeds, the store should contain the schema
  // metadata entry, the component metadata entry, and all of the update's
  // component hints.
  EXPECT_EQ(GetDBStoreEntryCount(), update_hint_count + 2);
  EXPECT_EQ(GetStoreHintEntryKeyCount(), update_hint_count);
  ExpectComponentHintsPresent(kUpdateComponentVersion, update_hint_count);
}

TEST_F(HintCacheStoreTest, CreateComponentDataWithAlreadyUpdatedVersionFails) {
  MetadataSchemaState schema_state = MetadataSchemaState::kValid;
  size_t initial_hint_count = 10;
  size_t update_hint_count = 5;
  SeedInitialData(schema_state, initial_hint_count);
  CreateDatabase();
  InitializeStore(schema_state);

  std::unique_ptr<HintUpdateData> update_data =
      hint_store()->MaybeCreateUpdateDataForComponentHints(
          base::Version(kUpdateComponentVersion));
  ASSERT_TRUE(update_data);
  SeedComponentUpdateData(update_data.get(), update_hint_count);
  UpdateComponentHints(std::move(update_data));

  // HintUpdateData for the component update should not be created for a second
  // component update with the same version as the first component update.
  EXPECT_FALSE(hint_store()->MaybeCreateUpdateDataForComponentHints(
      base::Version(kUpdateComponentVersion)));
}

TEST_F(HintCacheStoreTest, UpdateComponentHintsWithUpdatedVersionFails) {
  MetadataSchemaState schema_state = MetadataSchemaState::kValid;
  size_t initial_hint_count = 10;
  size_t update_hint_count_1 = 5;
  size_t update_hint_count_2 = 15;
  SeedInitialData(schema_state, initial_hint_count);
  CreateDatabase();
  InitializeStore(schema_state);

  // Create two updates for the same component version with different counts.
  std::unique_ptr<HintUpdateData> update_data_1 =
      hint_store()->MaybeCreateUpdateDataForComponentHints(
          base::Version(kUpdateComponentVersion));
  std::unique_ptr<HintUpdateData> update_data_2 =
      hint_store()->MaybeCreateUpdateDataForComponentHints(
          base::Version(kUpdateComponentVersion));
  ASSERT_TRUE(update_data_1);
  SeedComponentUpdateData(update_data_1.get(), update_hint_count_1);
  ASSERT_TRUE(update_data_2);
  SeedComponentUpdateData(update_data_2.get(), update_hint_count_2);

  // Update the component data with the same component version twice:
  // first with |update_data_1| and then with |update_data_2|.
  UpdateComponentHints(std::move(update_data_1));

  EXPECT_CALL(*this, OnUpdateHints());
  hint_store()->UpdateComponentHints(
      std::move(update_data_2),
      base::BindOnce(&HintCacheStoreTest::OnUpdateHints,
                     base::Unretained(this)));

  // Verify that the store is populated with the component data from
  // |update_data_1| and not |update_data_2|.
  EXPECT_EQ(GetDBStoreEntryCount(), update_hint_count_1 + 2);
  EXPECT_EQ(GetStoreHintEntryKeyCount(), update_hint_count_1);
  ExpectComponentHintsPresent(kUpdateComponentVersion, update_hint_count_1);
}

TEST_F(HintCacheStoreTest, LoadHintOnUnavailableStore) {
  size_t initial_hint_count = 10;
  SeedInitialData(MetadataSchemaState::kValid, initial_hint_count);
  CreateDatabase();

  const HintCacheStore::EntryKey kInvalidEntryKey = "invalid";
  hint_store()->LoadHint(kInvalidEntryKey,
                         base::BindOnce(&HintCacheStoreTest::OnHintLoaded,
                                        base::Unretained(this)));

  // Verify that the OnHintLoaded callback runs when the store is unavailable
  // and that both the key and the hint were correctly set in it.
  EXPECT_EQ(last_loaded_hint_entry_key(), kInvalidEntryKey);
  EXPECT_FALSE(last_loaded_hint());
}

TEST_F(HintCacheStoreTest, LoadHintFailure) {
  MetadataSchemaState schema_state = MetadataSchemaState::kValid;
  size_t hint_count = 10;
  SeedInitialData(schema_state, hint_count);
  CreateDatabase();
  InitializeStore(schema_state);

  const HintCacheStore::EntryKey kInvalidEntryKey = "invalid";
  hint_store()->LoadHint(kInvalidEntryKey,
                         base::BindOnce(&HintCacheStoreTest::OnHintLoaded,
                                        base::Unretained(this)));

  // OnLoadHint callback
  db()->GetCallback(false);

  // Verify that the OnHintLoaded callback runs when the store is unavailable
  // and that both the key and the hint were correctly set in it.
  EXPECT_EQ(last_loaded_hint_entry_key(), kInvalidEntryKey);
  EXPECT_FALSE(last_loaded_hint());
}

TEST_F(HintCacheStoreTest, LoadHintSuccessInitialData) {
  MetadataSchemaState schema_state = MetadataSchemaState::kValid;
  size_t hint_count = 10;
  SeedInitialData(schema_state, hint_count);
  CreateDatabase();
  InitializeStore(schema_state);

  // Verify that all component hints in the initial data can successfully be
  // loaded from the store.
  for (size_t i = 0; i < hint_count; ++i) {
    std::string host_suffix = GetHostSuffix(i);
    HintCacheStore::EntryKey hint_entry_key;
    if (!hint_store()->FindHintEntryKey(host_suffix, &hint_entry_key)) {
      FAIL() << "Hint entry not found for host suffix: " << host_suffix;
      continue;
    }

    hint_store()->LoadHint(hint_entry_key,
                           base::BindOnce(&HintCacheStoreTest::OnHintLoaded,
                                          base::Unretained(this)));

    // OnLoadHint callback
    db()->GetCallback(true);

    EXPECT_EQ(last_loaded_hint_entry_key(), hint_entry_key);
    if (!last_loaded_hint()) {
      FAIL() << "Loaded hint was null for entry key: " << hint_entry_key;
      continue;
    }

    EXPECT_EQ(last_loaded_hint()->key(), host_suffix);
  }
}

TEST_F(HintCacheStoreTest, LoadHintSuccessUpdateData) {
  MetadataSchemaState schema_state = MetadataSchemaState::kValid;
  size_t initial_hint_count = 10;
  size_t update_hint_count = 5;
  SeedInitialData(schema_state, initial_hint_count);
  CreateDatabase();
  InitializeStore(schema_state);

  std::unique_ptr<HintUpdateData> update_data =
      hint_store()->MaybeCreateUpdateDataForComponentHints(
          base::Version(kUpdateComponentVersion));
  ASSERT_TRUE(update_data);
  SeedComponentUpdateData(update_data.get(), update_hint_count);
  UpdateComponentHints(std::move(update_data));

  // Verify that all component hints within a successful component update can
  // be loaded from the store.
  for (size_t i = 0; i < update_hint_count; ++i) {
    std::string host_suffix = GetHostSuffix(i);
    HintCacheStore::EntryKey hint_entry_key;
    if (!hint_store()->FindHintEntryKey(host_suffix, &hint_entry_key)) {
      FAIL() << "Hint entry not found for host suffix: " << host_suffix;
      continue;
    }

    hint_store()->LoadHint(hint_entry_key,
                           base::BindOnce(&HintCacheStoreTest::OnHintLoaded,
                                          base::Unretained(this)));

    // OnLoadHint callback
    db()->GetCallback(true);

    EXPECT_EQ(last_loaded_hint_entry_key(), hint_entry_key);
    if (!last_loaded_hint()) {
      FAIL() << "Loaded hint was null for entry key: " << hint_entry_key;
      continue;
    }

    EXPECT_EQ(last_loaded_hint()->key(), host_suffix);
  }
}

TEST_F(HintCacheStoreTest, FindHintEntryKeyOnUnavailableStore) {
  size_t initial_hint_count = 10;
  SeedInitialData(MetadataSchemaState::kValid, initial_hint_count);
  CreateDatabase();

  std::string host_suffix = GetHostSuffix(0);
  HintCacheStore::EntryKey hint_entry_key;

  // Verify that hint entry keys can't be found when the store is unavailable.
  EXPECT_FALSE(hint_store()->FindHintEntryKey(host_suffix, &hint_entry_key));
}

TEST_F(HintCacheStoreTest, FindHintEntryKeyInitialData) {
  MetadataSchemaState schema_state = MetadataSchemaState::kValid;
  size_t hint_count = 10;
  SeedInitialData(schema_state, hint_count);
  CreateDatabase();
  InitializeStore(schema_state);

  // Verify that all hints contained within the initial store data are reported
  // as being found and hints that are not containd within the initial data are
  // properly reported as not being found.
  for (size_t i = 0; i < hint_count * 2; ++i) {
    std::string host_suffix = GetHostSuffix(i);
    HintCacheStore::EntryKey hint_entry_key;
    bool success = hint_store()->FindHintEntryKey(host_suffix, &hint_entry_key);
    EXPECT_EQ(success, i < hint_count);
  }
}

TEST_F(HintCacheStoreTest, FindHintEntryKeyUpdateData) {
  MetadataSchemaState schema_state = MetadataSchemaState::kValid;
  size_t initial_hint_count = 10;
  size_t update_hint_count = 5;
  SeedInitialData(schema_state, initial_hint_count);
  CreateDatabase();
  InitializeStore(schema_state);

  std::unique_ptr<HintUpdateData> update_data =
      hint_store()->MaybeCreateUpdateDataForComponentHints(
          base::Version(kUpdateComponentVersion));
  ASSERT_TRUE(update_data);
  SeedComponentUpdateData(update_data.get(), update_hint_count);
  UpdateComponentHints(std::move(update_data));

  // Verify that all hints contained within the component update are reported
  // by the store as being found and hints that are not containd within the
  // component update are properly reported as not being found.
  for (size_t i = 0; i < update_hint_count * 2; ++i) {
    std::string host_suffix = GetHostSuffix(i);
    HintCacheStore::EntryKey hint_entry_key;
    bool success = hint_store()->FindHintEntryKey(host_suffix, &hint_entry_key);
    EXPECT_EQ(success, i < update_hint_count);
  }
}

TEST_F(HintCacheStoreTest, FetchedHintsMetadataStored) {
  MetadataSchemaState schema_state = MetadataSchemaState::kValid;
  base::Time update_time = base::Time().Now();
  SeedInitialData(schema_state, 10, update_time);
  CreateDatabase();
  InitializeStore(schema_state);

  ExpectFetchedMetadata(update_time);
}

TEST_F(HintCacheStoreTest, FindHintEntryKeyForFetchedHints) {
  MetadataSchemaState schema_state = MetadataSchemaState::kValid;
  size_t update_hint_count = 5;
  base::Time update_time = base::Time().Now();
  SeedInitialData(schema_state, 0);
  CreateDatabase();
  InitializeStore(schema_state);

  std::unique_ptr<HintUpdateData> update_data =
      hint_store()->CreateUpdateDataForFetchedHints(
          update_time, update_time + optimization_guide::features::
                                         StoredFetchedHintsFreshnessDuration());
  ASSERT_TRUE(update_data);
  SeedFetchedUpdateData(update_data.get(), update_hint_count);
  UpdateFetchedHints(std::move(update_data));

  for (size_t i = 0; i < update_hint_count; ++i) {
    std::string host_suffix = GetHostSuffix(i);
    HintCacheStore::EntryKey hint_entry_key;
    bool success = hint_store()->FindHintEntryKey(host_suffix, &hint_entry_key);
    EXPECT_EQ(success, i < update_hint_count);
  }
}

TEST_F(HintCacheStoreTest, FindHintEntryKeyCheckFetchedBeforeComponentHints) {
  base::HistogramTester histogram_tester;
  MetadataSchemaState schema_state = MetadataSchemaState::kValid;
  size_t initial_hint_count = 10;
  base::Time update_time = base::Time().Now();
  SeedInitialData(schema_state, initial_hint_count);
  CreateDatabase();
  InitializeStore(schema_state);

  base::Version version("2.0.0");
  std::unique_ptr<HintUpdateData> update_data =
      hint_store()->MaybeCreateUpdateDataForComponentHints(
          base::Version(kUpdateComponentVersion));
  ASSERT_TRUE(update_data);

  proto::Hint hint1;
  hint1.set_key("domain1.org");
  hint1.set_key_representation(proto::HOST_SUFFIX);
  update_data->MoveHintIntoUpdateData(std::move(hint1));
  proto::Hint hint2;
  hint2.set_key("host.domain2.org");
  hint2.set_key_representation(proto::HOST_SUFFIX);
  update_data->MoveHintIntoUpdateData(std::move(hint2));

  UpdateComponentHints(std::move(update_data));

  // Add fetched hints to the store that overlap with the same hosts as the
  // initial set.
  update_data = hint_store()->CreateUpdateDataForFetchedHints(
      update_time,
      update_time +
          optimization_guide::features::StoredFetchedHintsFreshnessDuration());

  proto::Hint hint;
  hint.set_key("domain2.org");
  hint.set_key_representation(proto::HOST_SUFFIX);
  update_data->MoveHintIntoUpdateData(std::move(hint));

  UpdateFetchedHints(std::move(update_data));

  // Hint for host.domain2.org should be a fetched hint ("3_" prefix)
  // as fetched hints take priority.
  std::string host_suffix = "host.domain2.org";
  HintCacheStore::EntryKey hint_entry_key;
  if (!hint_store()->FindHintEntryKey(host_suffix, &hint_entry_key)) {
    FAIL() << "Hint entry not found for host suffix: " << host_suffix;
  }

  EXPECT_EQ(hint_entry_key, "3_domain2.org");

  host_suffix = "subdomain.domain1.org";

  if (!hint_store()->FindHintEntryKey(host_suffix, &hint_entry_key)) {
    FAIL() << "Hint entry not found for host suffix: " << host_suffix;
  }

  EXPECT_EQ(hint_entry_key, "2_2.0.0_domain1.org");
}

TEST_F(HintCacheStoreTest, ClearFetchedHints) {
  base::HistogramTester histogram_tester;
  MetadataSchemaState schema_state = MetadataSchemaState::kValid;
  size_t initial_hint_count = 10;
  base::Time update_time = base::Time().Now();
  SeedInitialData(schema_state, initial_hint_count);
  CreateDatabase();
  InitializeStore(schema_state);

  base::Version version("2.0.0");
  std::unique_ptr<HintUpdateData> update_data =
      hint_store()->MaybeCreateUpdateDataForComponentHints(
          base::Version(kUpdateComponentVersion));
  ASSERT_TRUE(update_data);

  proto::Hint hint1;
  hint1.set_key("domain1.org");
  hint1.set_key_representation(proto::HOST_SUFFIX);
  update_data->MoveHintIntoUpdateData(std::move(hint1));
  proto::Hint hint2;
  hint2.set_key("host.domain2.org");
  hint2.set_key_representation(proto::HOST_SUFFIX);
  update_data->MoveHintIntoUpdateData(std::move(hint2));

  UpdateComponentHints(std::move(update_data));

  // Add fetched hints to the store that overlap with the same hosts as the
  // initial set.
  update_data = hint_store()->CreateUpdateDataForFetchedHints(
      update_time, update_time + base::TimeDelta().FromDays(7));

  proto::Hint fetched_hint1;
  fetched_hint1.set_key("domain2.org");
  fetched_hint1.set_key_representation(proto::HOST_SUFFIX);
  update_data->MoveHintIntoUpdateData(std::move(fetched_hint1));
  proto::Hint fetched_hint2;
  fetched_hint2.set_key("domain3.org");
  fetched_hint2.set_key_representation(proto::HOST_SUFFIX);
  update_data->MoveHintIntoUpdateData(std::move(fetched_hint2));

  UpdateFetchedHints(std::move(update_data));

  // Hint for host.domain2.org should be a fetched hint ("3_" prefix)
  // as fetched hints take priority.
  std::string host_suffix = "host.domain2.org";
  HintCacheStore::EntryKey hint_entry_key;
  if (!hint_store()->FindHintEntryKey(host_suffix, &hint_entry_key)) {
    FAIL() << "Hint entry not found for host suffix: " << host_suffix;
  }

  EXPECT_EQ(hint_entry_key, "3_domain2.org");

  host_suffix = "subdomain.domain1.org";

  if (!hint_store()->FindHintEntryKey(host_suffix, &hint_entry_key)) {
    FAIL() << "Hint entry not found for host suffix: " << host_suffix;
  }

  EXPECT_EQ(hint_entry_key, "2_2.0.0_domain1.org");

  // Remove the fetched hints from the HintCacheStore.
  ClearFetchedHintsFromDatabase();

  host_suffix = "domain1.org";
  // Component hint should still exist.
  EXPECT_TRUE(hint_store()->FindHintEntryKey(host_suffix, &hint_entry_key));

  host_suffix = "domain3.org";
  // Fetched hint should not still exist.
  EXPECT_FALSE(hint_store()->FindHintEntryKey(host_suffix, &hint_entry_key));

  // Add Components back - newer version.
  base::Version version3("3.0.0");
  std::unique_ptr<HintUpdateData> update_data2 =
      hint_store()->MaybeCreateUpdateDataForComponentHints(version3);

  ASSERT_TRUE(update_data2);

  proto::Hint new_hint2;
  new_hint2.set_key("domain2.org");
  new_hint2.set_key_representation(proto::HOST_SUFFIX);
  update_data2->MoveHintIntoUpdateData(std::move(new_hint2));

  UpdateComponentHints(std::move(update_data2));

  host_suffix = "host.domain2.org";
  EXPECT_TRUE(hint_store()->FindHintEntryKey(host_suffix, &hint_entry_key));

  update_data = hint_store()->CreateUpdateDataForFetchedHints(
      update_time,
      update_time +
          optimization_guide::features::StoredFetchedHintsFreshnessDuration());
  proto::Hint new_hint;
  new_hint.set_key("domain1.org");
  new_hint.set_key_representation(proto::HOST_SUFFIX);
  update_data->MoveHintIntoUpdateData(std::move(new_hint));

  UpdateFetchedHints(std::move(update_data));

  // Add fetched hints to the store that overlap with the same hosts as the
  // initial set.
  host_suffix = "subdomain.domain1.org";

  if (!hint_store()->FindHintEntryKey(host_suffix, &hint_entry_key)) {
    FAIL() << "Hint entry not found for host suffix: " << host_suffix;
  }

  EXPECT_EQ(hint_entry_key, "3_domain1.org");
}

TEST_F(HintCacheStoreTest, FetchHintsPurgeExpiredFetchedHints) {
  base::HistogramTester histogram_tester;
  MetadataSchemaState schema_state = MetadataSchemaState::kValid;
  size_t initial_hint_count = 10;
  base::Time update_time = base::Time().Now();
  SeedInitialData(schema_state, initial_hint_count);
  CreateDatabase();
  InitializeStore(schema_state);

  base::Version version("2.0.0");
  std::unique_ptr<HintUpdateData> update_data =
      hint_store()->MaybeCreateUpdateDataForComponentHints(
          base::Version(kUpdateComponentVersion));
  ASSERT_TRUE(update_data);

  proto::Hint hint1;
  hint1.set_key("domain1.org");
  hint1.set_key_representation(proto::HOST_SUFFIX);
  update_data->MoveHintIntoUpdateData(std::move(hint1));
  proto::Hint hint2;
  hint2.set_key("host.domain2.org");
  hint2.set_key_representation(proto::HOST_SUFFIX);
  update_data->MoveHintIntoUpdateData(std::move(hint2));

  UpdateComponentHints(std::move(update_data));

  // Add fetched hints to the store that overlap with the same hosts as the
  // initial set.
  update_data = hint_store()->CreateUpdateDataForFetchedHints(
      update_time, update_time + base::TimeDelta().FromDays(7));

  proto::Hint fetched_hint1;
  fetched_hint1.set_key("domain2.org");
  fetched_hint1.set_key_representation(proto::HOST_SUFFIX);
  update_data->MoveHintIntoUpdateData(std::move(fetched_hint1));
  proto::Hint fetched_hint2;
  fetched_hint2.set_key("domain3.org");
  fetched_hint2.set_key_representation(proto::HOST_SUFFIX);
  update_data->MoveHintIntoUpdateData(std::move(fetched_hint2));

  UpdateFetchedHints(std::move(update_data));

  // Add expired fetched hints to the store.
  update_data = hint_store()->CreateUpdateDataForFetchedHints(
      update_time, update_time - base::TimeDelta().FromDays(7));

  proto::Hint fetched_hint3;
  fetched_hint1.set_key("domain4.org");
  fetched_hint1.set_key_representation(proto::HOST_SUFFIX);
  update_data->MoveHintIntoUpdateData(std::move(fetched_hint1));
  proto::Hint fetched_hint4;
  fetched_hint2.set_key("domain5.org");
  fetched_hint2.set_key_representation(proto::HOST_SUFFIX);
  update_data->MoveHintIntoUpdateData(std::move(fetched_hint2));

  UpdateFetchedHints(std::move(update_data));

  PurgeExpiredFetchedHints();

  HintCacheStore::EntryKey hint_entry_key;
  EXPECT_FALSE(hint_store()->FindHintEntryKey("domain4.org", &hint_entry_key));
  EXPECT_FALSE(hint_store()->FindHintEntryKey("domain5.org", &hint_entry_key));
  EXPECT_TRUE(hint_store()->FindHintEntryKey("domain2.org", &hint_entry_key));
  EXPECT_TRUE(hint_store()->FindHintEntryKey("domain3.org", &hint_entry_key));
}

TEST_F(HintCacheStoreTest, LoadingHintUpdatesPrefCorrectly) {
  base::HistogramTester histogram_tester;

  // Update component with some hints.
  MetadataSchemaState schema_state = MetadataSchemaState::kValid;
  size_t initial_hint_count = 10;
  size_t update_hint_count = 5;
  SeedInitialData(schema_state, initial_hint_count);
  CreateDatabase();
  InitializeStore(schema_state);

  std::unique_ptr<HintUpdateData> update_data =
      hint_store()->MaybeCreateUpdateDataForComponentHints(
          base::Version(kUpdateComponentVersion));
  ASSERT_TRUE(update_data);
  SeedComponentUpdateData(update_data.get(), update_hint_count);
  UpdateComponentHints(std::move(update_data));

  // Also seed a fetched hint into the store.
  update_data = hint_store()->CreateUpdateDataForFetchedHints(
      base::Time().Now(),
      base::Time().Now() +
          optimization_guide::features::StoredFetchedHintsFreshnessDuration());
  proto::Hint hint;
  hint.set_key("domain2.org");
  hint.set_key_representation(proto::HOST_SUFFIX);
  update_data->MoveHintIntoUpdateData(std::move(hint));
  UpdateFetchedHints(std::move(update_data));

  // Load hint for component host suffix.
  std::string host_suffix1 = GetHostSuffix(0);
  HintCacheStore::EntryKey hint_entry_key1;
  if (!hint_store()->FindHintEntryKey(host_suffix1, &hint_entry_key1)) {
    FAIL() << "Hint entry not found for host suffix: " << host_suffix1;
  }
  hint_store()->LoadHint(hint_entry_key1,
                         base::BindOnce(&HintCacheStoreTest::OnHintLoaded,
                                        base::Unretained(this)));
  db()->GetCallback(true);
  EXPECT_TRUE(hint_loaded_counts_pref()->HasKey("Total"));
  EXPECT_EQ(1, hint_loaded_counts_pref()->FindIntKey("Total").value());
  EXPECT_TRUE(hint_loaded_counts_pref()->HasKey("ComponentHint"));
  EXPECT_EQ(1, hint_loaded_counts_pref()->FindIntKey("ComponentHint").value());

  // Load another component hint.
  std::string host_suffix2 = GetHostSuffix(1);
  HintCacheStore::EntryKey hint_entry_key2;
  if (!hint_store()->FindHintEntryKey(host_suffix2, &hint_entry_key2)) {
    FAIL() << "Hint entry not found for host suffix: " << host_suffix2;
  }
  hint_store()->LoadHint(hint_entry_key2,
                         base::BindOnce(&HintCacheStoreTest::OnHintLoaded,
                                        base::Unretained(this)));
  db()->GetCallback(true);
  EXPECT_TRUE(hint_loaded_counts_pref()->HasKey("Total"));
  EXPECT_EQ(2, hint_loaded_counts_pref()->FindIntKey("Total").value());
  EXPECT_TRUE(hint_loaded_counts_pref()->HasKey("ComponentHint"));
  EXPECT_EQ(2, hint_loaded_counts_pref()->FindIntKey("ComponentHint").value());

  // Load hint for a fetched hint.
  std::string fetched_host_suffix = "host.domain2.org";
  HintCacheStore::EntryKey fetched_hint_entry_key;
  if (!hint_store()->FindHintEntryKey(fetched_host_suffix,
                                      &fetched_hint_entry_key)) {
    FAIL() << "Hint entry not found for host suffix: " << fetched_host_suffix;
  }
  hint_store()->LoadHint(fetched_hint_entry_key,
                         base::BindOnce(&HintCacheStoreTest::OnHintLoaded,
                                        base::Unretained(this)));
  db()->GetCallback(true);
  EXPECT_TRUE(hint_loaded_counts_pref()->HasKey("Total"));
  EXPECT_EQ(3, hint_loaded_counts_pref()->FindIntKey("Total").value());
  EXPECT_TRUE(hint_loaded_counts_pref()->HasKey("ComponentHint"));
  EXPECT_EQ(2, hint_loaded_counts_pref()->FindIntKey("ComponentHint").value());
  EXPECT_TRUE(hint_loaded_counts_pref()->HasKey("FetchedHint"));
  EXPECT_EQ(1, hint_loaded_counts_pref()->FindIntKey("FetchedHint").value());

  // Load hint for a hint entry we do not have.
  hint_store()->LoadHint("doesnotexist",
                         base::BindOnce(&HintCacheStoreTest::OnHintLoaded,
                                        base::Unretained(this)));
  db()->GetCallback(true);
  EXPECT_TRUE(hint_loaded_counts_pref()->HasKey("Total"));
  EXPECT_EQ(4, hint_loaded_counts_pref()->FindIntKey("Total").value());
  EXPECT_TRUE(hint_loaded_counts_pref()->HasKey("ComponentHint"));
  EXPECT_EQ(2, hint_loaded_counts_pref()->FindIntKey("ComponentHint").value());
  EXPECT_TRUE(hint_loaded_counts_pref()->HasKey("FetchedHint"));
  EXPECT_EQ(1, hint_loaded_counts_pref()->FindIntKey("FetchedHint").value());

  // Now update the component with a new version.
  std::unique_ptr<HintUpdateData> update_data2 =
      hint_store()->MaybeCreateUpdateDataForComponentHints(
          base::Version("100.0.0"));
  ASSERT_TRUE(update_data2);
  SeedComponentUpdateData(update_data2.get(), update_hint_count);
  UpdateComponentHints(std::move(update_data2));

  histogram_tester.ExpectTotalCount("OptimizationGuide.HintsLoadedPercentage",
                                    1);
  histogram_tester.ExpectUniqueSample("OptimizationGuide.HintsLoadedPercentage",
                                      75, 1);
  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.HintsLoadedPercentage.ComponentHint", 1);
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.HintsLoadedPercentage.ComponentHint", 50, 1);
  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.HintsLoadedPercentage.FetchedHint", 1);
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.HintsLoadedPercentage.FetchedHint", 25, 1);

  // Ensure that pref has been cleared.
  EXPECT_TRUE(hint_loaded_counts_pref()->empty());
}

TEST_F(HintCacheStoreTest, FetchedHintsLoadExpiredHint) {
  base::HistogramTester histogram_tester;
  MetadataSchemaState schema_state = MetadataSchemaState::kValid;
  size_t initial_hint_count = 10;
  base::Time update_time = base::Time().Now();
  SeedInitialData(schema_state, initial_hint_count);
  CreateDatabase();
  InitializeStore(schema_state);

  base::Version version("2.0.0");
  std::unique_ptr<HintUpdateData> update_data =
      hint_store()->MaybeCreateUpdateDataForComponentHints(
          base::Version(kUpdateComponentVersion));
  ASSERT_TRUE(update_data);

  proto::Hint hint1;
  hint1.set_key("domain1.org");
  hint1.set_key_representation(proto::HOST_SUFFIX);
  update_data->MoveHintIntoUpdateData(std::move(hint1));
  proto::Hint hint2;
  hint2.set_key("host.domain2.org");
  hint2.set_key_representation(proto::HOST_SUFFIX);
  update_data->MoveHintIntoUpdateData(std::move(hint2));

  UpdateComponentHints(std::move(update_data));

  // Add fetched hints to the store that expired.
  update_data = hint_store()->CreateUpdateDataForFetchedHints(
      update_time, update_time - base::TimeDelta().FromDays(10));

  proto::Hint fetched_hint1;
  fetched_hint1.set_key("domain2.org");
  fetched_hint1.set_key_representation(proto::HOST_SUFFIX);
  update_data->MoveHintIntoUpdateData(std::move(fetched_hint1));
  proto::Hint fetched_hint2;
  fetched_hint2.set_key("domain3.org");
  fetched_hint2.set_key_representation(proto::HOST_SUFFIX);
  update_data->MoveHintIntoUpdateData(std::move(fetched_hint2));

  UpdateFetchedHints(std::move(update_data));

  // Hint for host.domain2.org should be a fetched hint ("3_" prefix)
  // as fetched hints take priority.
  std::string host_suffix = "host.domain2.org";
  HintCacheStore::EntryKey hint_entry_key;
  if (!hint_store()->FindHintEntryKey(host_suffix, &hint_entry_key)) {
    FAIL() << "Hint entry not found for host suffix: " << host_suffix;
  }
  EXPECT_EQ(hint_entry_key, "3_domain2.org");
  hint_store()->LoadHint(hint_entry_key,
                         base::BindOnce(&HintCacheStoreTest::OnHintLoaded,
                                        base::Unretained(this)));

  // OnLoadHint callback
  db()->GetCallback(true);

  // |hint_entry_key| will be a fetched hint but the entry will be empty.
  EXPECT_EQ(last_loaded_hint_entry_key(), hint_entry_key);
  EXPECT_FALSE(last_loaded_hint());
  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.HintCacheStore.OnLoadHint.FetchedHintExpired", true,
      1);
}

}  // namespace optimization_guide
