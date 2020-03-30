// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/structured/key_data.h"

#include <memory>
#include <string>

#include "base/containers/flat_set.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_mock_clock_override.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "components/metrics/structured/event_base.h"
#include "components/metrics/structured/histogram_util.h"
#include "components/metrics/structured/recorder.h"
#include "components/metrics/structured/structured_events.h"
#include "components/prefs/json_pref_store.h"
#include "components/prefs/persistent_pref_store.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace metrics {
namespace structured {
namespace internal {

namespace {

// 32 byte long test key, matching the size of a real key.
constexpr char kKey[] = "abcdefghijklmnopqrstuvwxyzabcdef";

// These event and metric names are used for testing.
// - event: TestEventOne
//   - metric: TestValueOne
//   - metric: TestValueTwo
// - event: TestEventTwo
//   - metric: TestValueOne

// The name hash of "TestEventOne".
constexpr uint64_t kEventOneHash = UINT64_C(15619026293081468407);
// The name hash of "TestEventTwo".
constexpr uint64_t kEventTwoHash = UINT64_C(15791833939776536363);
// The name hash of "TestProject".
constexpr uint64_t kProjectHash = UINT64_C(17426425568333718899);

// The name hash of "TestMetricOne".
constexpr uint64_t kMetricOneHash = UINT64_C(637929385654885975);
// The name hash of "TestMetricTwo".
constexpr uint64_t kMetricTwoHash = UINT64_C(14083999144141567134);

// The hex-encoded frst 8 bytes of SHA256(kKey), ie. the user ID for key kKey.
constexpr char kUserId[] = "2070DF23E0D95759";

// Test values and their hashes. Hashes are the first 8 bytes of:
//
// HMAC_SHA256(concat(hex(kMetricNHash), kValueN),
// "abcdefghijklmnopqrstuvwxyzabcdef")
constexpr char kValueOne[] = "value one";
constexpr char kValueTwo[] = "value two";
constexpr char kValueOneHash[] = "805B8790DC69B773";
constexpr char kValueTwoHash[] = "87CEF12FB15E0B3A";

std::string HashToHex(const uint64_t hash) {
  return base::HexEncode(&hash, sizeof(uint64_t));
}

std::string KeyPath(const uint64_t event) {
  return base::StrCat({"keys.", base::NumberToString(event), ".key"});
}

std::string LastRotationPath(const uint64_t event) {
  return base::StrCat({"keys.", base::NumberToString(event), ".last_rotation"});
}

std::string RotationPeriodPath(const uint64_t event) {
  return base::StrCat(
      {"keys.", base::NumberToString(event), ".rotation_period"});
}

// Returns the total number of events registered in structured.xml. This is used
// to determine how many keys we expect to load or rotate on initialization.
int NumberOfEvents() {
  return sizeof(metrics::structured::events::kProjectNameHashes) /
         sizeof(uint64_t);
}

}  // namespace

class KeyDataTest : public testing::Test {
 protected:
  void SetUp() override { ASSERT_TRUE(temp_dir_.CreateUniqueTempDir()); }

  void StandardSetup() {
    MakeKeyStore();
    MakeKeyData();
    CommitKeyStore();
  }

  void ResetState() {
    key_data_.reset();
    key_store_.reset();
    base::DeleteFile(GetKeyStorePath(), false);
    ASSERT_FALSE(base::PathExists(GetKeyStorePath()));
  }

  void MakeKeyStore() {
    key_store_ = new JsonPrefStore(GetKeyStorePath());
    key_store_->ReadPrefs();
  }

  void MakeKeyData() { key_data_ = std::make_unique<KeyData>(GetKeyStore()); }

  void CommitKeyStore() {
    key_store_->CommitPendingWrite();
    Wait();
    ASSERT_TRUE(base::PathExists(GetKeyStorePath()));
  }

  JsonPrefStore* GetKeyStore() { return key_store_.get(); }

  base::FilePath GetKeyStorePath() {
    return temp_dir_.GetPath().Append("keys.json");
  }

  std::string GetString(const std::string& path) {
    const base::Value* value;
    GetKeyStore()->GetValue(path, &value);
    return value->GetString();
  }

  int GetInt(const std::string& path) {
    const base::Value* value;
    GetKeyStore()->GetValue(path, &value);
    return value->GetInt();
  }

  void SetString(const std::string& path, const std::string& value) {
    key_store_->SetValue(path, std::make_unique<base::Value>(value),
                         WriteablePrefStore::DEFAULT_PREF_WRITE_FLAGS);
    CommitKeyStore();
  }

  void SetInt(const std::string& path, const int value) {
    key_store_->SetValue(path, std::make_unique<base::Value>(value),
                         WriteablePrefStore::DEFAULT_PREF_WRITE_FLAGS);
    CommitKeyStore();
  }

  void SetKeyData(const uint64_t event,
                  const std::string& key,
                  const int last_rotation,
                  const int rotation_period) {
    SetString(KeyPath(event), key);
    SetInt(LastRotationPath(event), last_rotation);
    SetInt(RotationPeriodPath(event), rotation_period);
  }

  void Wait() { task_environment_.RunUntilIdle(); }

  void ExpectNoErrors() {
    histogram_tester_.ExpectTotalCount("UMA.StructuredMetrics.InternalError",
                                       0);
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::UI,
      base::test::TaskEnvironment::ThreadPoolExecutionMode::QUEUED};
  base::ScopedTempDir temp_dir_;
  base::ScopedMockClockOverride time_;
  base::HistogramTester histogram_tester_;
  scoped_refptr<JsonPrefStore> key_store_;

  std::unique_ptr<KeyData> key_data_;
};

// If there is no key store file present, check that new keys are generated for
// each event, and those keys are of the right length and different from each
// other.
TEST_F(KeyDataTest, GeneratesKeysForEvents) {
  StandardSetup();
  histogram_tester_.ExpectUniqueSample(
      "UMA.StructuredMetrics.KeyValidationState", KeyValidationState::kCreated,
      NumberOfEvents());

  const std::string key_one = GetString(KeyPath(kEventOneHash));
  const std::string key_two = GetString(KeyPath(kProjectHash));

  EXPECT_EQ(key_one.size(), 32ul);
  EXPECT_EQ(key_two.size(), 32ul);
  EXPECT_NE(key_one, key_two);
}

// When repeatedly initialized with no key store file present, ensure the keys
// generated each time are distinct.
TEST_F(KeyDataTest, GeneratesDistinctKeys) {
  base::flat_set<std::string> keys;

  for (int i = 0; i < 10; ++i) {
    ResetState();
    StandardSetup();
    keys.insert(GetString(KeyPath(kEventOneHash)));
    histogram_tester_.ExpectUniqueSample(
        "UMA.StructuredMetrics.KeyValidationState",
        KeyValidationState::kCreated, NumberOfEvents() * (i + 1));
  }

  EXPECT_EQ(keys.size(), 10ul);
}

// If there is an existing key store file, check that its keys are not replaced.
TEST_F(KeyDataTest, ReuseExistingKeys) {
  StandardSetup();
  histogram_tester_.ExpectBucketCount(
      "UMA.StructuredMetrics.KeyValidationState", KeyValidationState::kCreated,
      NumberOfEvents());
  const std::string key_one = GetString(KeyPath(kEventOneHash));
  CommitKeyStore();

  key_data_.reset();
  key_store_.reset();
  StandardSetup();
  histogram_tester_.ExpectBucketCount(
      "UMA.StructuredMetrics.KeyValidationState", KeyValidationState::kValid,
      NumberOfEvents());
  const std::string key_two = GetString(KeyPath(kEventOneHash));

  EXPECT_EQ(key_one, key_two);
}

// Check that different events have different hashes for the same metric and
// value.
TEST_F(KeyDataTest, DifferentEventsDifferentHashes) {
  StandardSetup();
  // Even though
  EXPECT_NE(
      key_data_->HashForEventMetric(kEventOneHash, kMetricOneHash, "value"),
      key_data_->HashForEventMetric(kEventTwoHash, kMetricOneHash, "value"));
  ExpectNoErrors();
}

// Check that an event has different hashes for different values of the same
// metric.
TEST_F(KeyDataTest, DifferentMetricsDifferentHashes) {
  StandardSetup();
  EXPECT_NE(
      key_data_->HashForEventMetric(kEventOneHash, kMetricOneHash, "first"),
      key_data_->HashForEventMetric(kEventOneHash, kMetricOneHash, "second"));
  ExpectNoErrors();
}

// Check that an event has different hashes for different metrics with the same
// value.
TEST_F(KeyDataTest, DifferentValuesDifferentHashes) {
  StandardSetup();
  EXPECT_NE(
      key_data_->HashForEventMetric(kEventOneHash, kMetricOneHash, "value"),
      key_data_->HashForEventMetric(kEventOneHash, kMetricTwoHash, "value"));
  ExpectNoErrors();
}

// Ensure that KeyData::UserId is the expected value of SHA256(key).
TEST_F(KeyDataTest, CheckUserIDs) {
  MakeKeyStore();
  SetKeyData(kEventOneHash, kKey, 0, 90);
  CommitKeyStore();

  MakeKeyData();
  EXPECT_EQ(HashToHex(key_data_->UserEventId(kEventOneHash)), kUserId);
  EXPECT_NE(HashToHex(key_data_->UserEventId(kEventTwoHash)), kUserId);
  ExpectNoErrors();
}

// Ensure that KeyData::Hash returns expected values for a known key and value.
TEST_F(KeyDataTest, CheckHashes) {
  MakeKeyStore();
  SetString(KeyPath(kEventOneHash), kKey);
  SetKeyData(kEventOneHash, kKey, 0, 90);
  CommitKeyStore();

  MakeKeyData();
  EXPECT_EQ(HashToHex(key_data_->HashForEventMetric(kEventOneHash,
                                                    kMetricOneHash, kValueOne)),
            kValueOneHash);
  EXPECT_EQ(HashToHex(key_data_->HashForEventMetric(kEventOneHash,
                                                    kMetricTwoHash, kValueTwo)),
            kValueTwoHash);
  ExpectNoErrors();
}

// Check that keys for a event are correctly rotated after the default 90 day
// rotation period.
TEST_F(KeyDataTest, KeysRotated) {
  // This test intentionally doesn't test the key validation metric. Events can
  // have custom rotation periods, so this test could rotate some arbitrary set
  // of keys that we don't know ahead of time, which would require too much test
  // logic.

  StandardSetup();
  const uint64_t first_id = key_data_->UserEventId(kEventOneHash);
  const int start_day = (base::Time::Now() - base::Time::UnixEpoch()).InDays();

  // TestEventOne has a default rotation period of 90 days.
  EXPECT_EQ(GetInt(RotationPeriodPath(kEventOneHash)), 90);

  // Set the last rotation to today for testing.
  SetInt(LastRotationPath(kEventOneHash), start_day);

  {
    // Advancing by 50 days, the key should not be rotated.
    key_data_.reset();
    time_.Advance(base::TimeDelta::FromDays(50));
    StandardSetup();
    EXPECT_EQ(key_data_->UserEventId(kEventOneHash), first_id);
    EXPECT_EQ(GetInt(LastRotationPath(kEventOneHash)), start_day);
  }

  {
    // Advancing by another 50 days, the key should be rotated and the last
    // rotation day should be incremented by 90.
    key_data_.reset();
    time_.Advance(base::TimeDelta::FromDays(50));
    StandardSetup();
    EXPECT_NE(key_data_->UserEventId(kEventOneHash), first_id);
    EXPECT_EQ(GetInt(LastRotationPath(kEventOneHash)), start_day + 90);
  }

  {
    // Advancing by 453 days, the last rotation day should now 6 periods of 90
    // days ahead.
    key_data_.reset();
    time_.Advance(base::TimeDelta::FromDays(453));
    StandardSetup();
    EXPECT_EQ(GetInt(LastRotationPath(kEventOneHash)), start_day + 6 * 90);
  }
}

}  // namespace internal
}  // namespace structured
}  // namespace metrics
