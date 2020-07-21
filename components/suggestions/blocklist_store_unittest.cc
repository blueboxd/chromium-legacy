// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/suggestions/blocklist_store.h"

#include <memory>
#include <set>
#include <string>

#include "base/macros.h"
#include "components/suggestions/proto/suggestions.pb.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "testing/gtest/include/gtest/gtest.h"

using sync_preferences::TestingPrefServiceSyncable;

namespace suggestions {

namespace {

const char kTestUrlA[] = "http://aaa.com/";
const char kTestUrlB[] = "http://bbb.com/";
const char kTestUrlC[] = "http://ccc.com/";
const char kTestUrlD[] = "http://ddd.com/";

SuggestionsProfile CreateSuggestions(std::set<std::string> urls) {
  SuggestionsProfile suggestions;
  for (auto& url : urls) {
    ChromeSuggestion* suggestion = suggestions.add_suggestions();
    suggestion->set_url(url);
  }
  suggestions.set_timestamp(123);
  return suggestions;
}

void ValidateSuggestions(const SuggestionsProfile& expected,
                         const SuggestionsProfile& actual) {
  ASSERT_EQ(expected.suggestions_size(), actual.suggestions_size());
  for (int i = 0; i < expected.suggestions_size(); ++i) {
    EXPECT_EQ(expected.suggestions(i).url(), actual.suggestions(i).url());
    EXPECT_EQ(expected.suggestions(i).title(), actual.suggestions(i).title());
    EXPECT_EQ(expected.suggestions(i).favicon_url(),
              actual.suggestions(i).favicon_url());
  }
}

}  // namespace

class BlocklistStoreTest : public testing::Test {
 public:
  BlocklistStoreTest()
      : pref_service_(new sync_preferences::TestingPrefServiceSyncable) {}

  void SetUp() override {
    BlocklistStore::RegisterProfilePrefs(pref_service()->registry());
  }

  sync_preferences::TestingPrefServiceSyncable* pref_service() {
    return pref_service_.get();
  }

 private:
  std::unique_ptr<sync_preferences::TestingPrefServiceSyncable> pref_service_;

  DISALLOW_COPY_AND_ASSIGN(BlocklistStoreTest);
};

// Tests adding, removing to the blocklist and filtering.
TEST_F(BlocklistStoreTest, BasicInteractions) {
  BlocklistStore blocklist_store(pref_service());

  // Create suggestions with A, B and C. C and D will be added to the blocklist.
  std::set<std::string> suggested_urls;
  suggested_urls.insert(kTestUrlA);
  suggested_urls.insert(kTestUrlB);
  const SuggestionsProfile suggestions_filtered =
      CreateSuggestions(suggested_urls);
  suggested_urls.insert(kTestUrlC);
  const SuggestionsProfile original_suggestions =
      CreateSuggestions(suggested_urls);
  SuggestionsProfile suggestions;

  // Filter with an empty blocklist.
  suggestions.CopyFrom(original_suggestions);
  blocklist_store.FilterSuggestions(&suggestions);
  ValidateSuggestions(original_suggestions, suggestions);

  // Add C and D to the blocklist and filter.
  suggestions.CopyFrom(original_suggestions);
  EXPECT_TRUE(blocklist_store.BlocklistUrl(GURL(kTestUrlC)));
  EXPECT_TRUE(blocklist_store.BlocklistUrl(GURL(kTestUrlD)));
  blocklist_store.FilterSuggestions(&suggestions);
  ValidateSuggestions(suggestions_filtered, suggestions);

  // Remove C from the blocklist and filter.
  suggestions.CopyFrom(original_suggestions);
  EXPECT_TRUE(blocklist_store.RemoveUrl(GURL(kTestUrlC)));
  blocklist_store.FilterSuggestions(&suggestions);
  ValidateSuggestions(original_suggestions, suggestions);
}

TEST_F(BlocklistStoreTest, BlocklistTwiceSuceeds) {
  BlocklistStore blocklist_store(pref_service());
  EXPECT_TRUE(blocklist_store.BlocklistUrl(GURL(kTestUrlA)));
  EXPECT_TRUE(blocklist_store.BlocklistUrl(GURL(kTestUrlA)));
}

TEST_F(BlocklistStoreTest, RemoveUnknownUrlFails) {
  BlocklistStore blocklist_store(pref_service());
  EXPECT_FALSE(blocklist_store.RemoveUrl(GURL(kTestUrlA)));
}

TEST_F(BlocklistStoreTest, TestGetTimeUntilReadyForUpload) {
  // Tests assumes completion within 1 hour.
  base::TimeDelta upload_delay = base::TimeDelta::FromHours(1);
  base::TimeDelta no_delay = base::TimeDelta::FromHours(0);
  std::unique_ptr<BlocklistStore> blocklist_store(
      new BlocklistStore(pref_service(), upload_delay));
  base::TimeDelta candidate_delta;

  // Blocklist is empty.
  EXPECT_FALSE(blocklist_store->GetTimeUntilReadyForUpload(&candidate_delta));
  EXPECT_FALSE(blocklist_store->GetTimeUntilURLReadyForUpload(
      GURL(kTestUrlA), &candidate_delta));

  // Blocklist contains kTestUrlA.
  EXPECT_TRUE(blocklist_store->BlocklistUrl(GURL(kTestUrlA)));
  candidate_delta = upload_delay + base::TimeDelta::FromDays(1);
  EXPECT_TRUE(blocklist_store->GetTimeUntilReadyForUpload(&candidate_delta));
  EXPECT_LE(candidate_delta, upload_delay);
  EXPECT_GE(candidate_delta, no_delay);
  candidate_delta = upload_delay + base::TimeDelta::FromDays(1);
  EXPECT_TRUE(blocklist_store->GetTimeUntilURLReadyForUpload(GURL(kTestUrlA),
                                                             &candidate_delta));
  EXPECT_LE(candidate_delta, upload_delay);
  EXPECT_GE(candidate_delta, no_delay);
  EXPECT_FALSE(blocklist_store->GetTimeUntilURLReadyForUpload(
      GURL(kTestUrlB), &candidate_delta));

  // There should be no candidate for upload since the upload delay is 1 day.
  // Note: this is a test that relies on timing.
  GURL retrieved;
  EXPECT_FALSE(blocklist_store->GetCandidateForUpload(&retrieved));

  // Same, but with an upload delay of 0.
  blocklist_store = std::make_unique<BlocklistStore>(pref_service(), no_delay);
  EXPECT_TRUE(blocklist_store->BlocklistUrl(GURL(kTestUrlA)));
  candidate_delta = no_delay + base::TimeDelta::FromDays(1);
  EXPECT_TRUE(blocklist_store->GetTimeUntilReadyForUpload(&candidate_delta));
  EXPECT_EQ(candidate_delta, no_delay);
  candidate_delta = no_delay + base::TimeDelta::FromDays(1);
  EXPECT_TRUE(blocklist_store->GetTimeUntilURLReadyForUpload(GURL(kTestUrlA),
                                                             &candidate_delta));
  EXPECT_EQ(candidate_delta, no_delay);
}

TEST_F(BlocklistStoreTest, GetCandidateForUpload) {
  BlocklistStore blocklist_store(pref_service(), base::TimeDelta::FromDays(0));
  // Empty blocklist.
  GURL retrieved;
  EXPECT_FALSE(blocklist_store.GetCandidateForUpload(&retrieved));

  // Blocklist A and B. Expect to retrieve A or B.
  EXPECT_TRUE(blocklist_store.BlocklistUrl(GURL(kTestUrlA)));
  EXPECT_TRUE(blocklist_store.BlocklistUrl(GURL(kTestUrlB)));
  EXPECT_TRUE(blocklist_store.GetCandidateForUpload(&retrieved));
  std::string retrieved_string = retrieved.spec();
  EXPECT_TRUE(retrieved_string == std::string(kTestUrlA) ||
              retrieved_string == std::string(kTestUrlB));
}

}  // namespace suggestions
