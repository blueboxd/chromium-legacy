// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/engine_impl/commit_processor.h"

#include <memory>

#include "components/sync/engine_impl/commit_contribution.h"
#include "components/sync/engine_impl/commit_contributor.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {
namespace {

constexpr int kMaxEntries = 17;

using testing::_;
using testing::Pair;
using testing::UnorderedElementsAre;

MATCHER_P(HasNumEntries, num_entries, "") {
  return static_cast<int>(arg->GetNumEntries()) == num_entries;
}

// Simple implementation of CommitContribution that only implements
// GetNumEntries().
class FakeCommitContribution : public CommitContribution {
 public:
  explicit FakeCommitContribution(size_t num_entries)
      : num_entries_(num_entries) {}

  ~FakeCommitContribution() override = default;

  // CommitContribution implementation.
  void AddToCommitMessage(sync_pb::ClientToServerMessage* msg) override {}
  SyncerError ProcessCommitResponse(
      const sync_pb::ClientToServerResponse& response,
      StatusController* status) override {
    return SyncerError();
  }
  void ProcessCommitFailure(SyncCommitError commit_error) override {}
  void CleanUp() override {}
  size_t GetNumEntries() const override { return num_entries_; }

 private:
  const size_t num_entries_;
};

ACTION_P(ReturnContributionWithEntries, num_entries) {
  return std::make_unique<FakeCommitContribution>(num_entries);
}

class MockCommitContributor : public CommitContributor {
 public:
  MockCommitContributor() = default;
  ~MockCommitContributor() override = default;

  MOCK_METHOD1(GetContribution,
               std::unique_ptr<CommitContribution>(size_t max_entries));
};

class CommitProcessorTest : public testing::Test {
 protected:
  CommitProcessorTest()
      : contributor_map_{{NIGORI, &nigori_contributor_},
                         {SHARING_MESSAGE, &sharing_message_contributor_},
                         {BOOKMARKS, &bookmark_contributor_},
                         {PREFERENCES, &preference_contributor_}},
        processor_(/*commit_types=*/ModelTypeSet{NIGORI, SHARING_MESSAGE,
                                                 BOOKMARKS, PREFERENCES},
                   &contributor_map_) {
    EXPECT_TRUE(PriorityUserTypes().Has(SHARING_MESSAGE));
    EXPECT_FALSE(PriorityUserTypes().Has(BOOKMARKS));
    EXPECT_FALSE(PriorityUserTypes().Has(PREFERENCES));
  }

  testing::NiceMock<MockCommitContributor> nigori_contributor_;

  // A priority user type.
  testing::NiceMock<MockCommitContributor> sharing_message_contributor_;

  // Regular user types.
  testing::NiceMock<MockCommitContributor> bookmark_contributor_;
  testing::NiceMock<MockCommitContributor> preference_contributor_;

  CommitContributorMap contributor_map_;
  CommitProcessor processor_;
};

TEST_F(CommitProcessorTest, ShouldGatherNigoriOnlyContribution) {
  EXPECT_CALL(nigori_contributor_, GetContribution(kMaxEntries))
      .WillOnce(ReturnContributionWithEntries(/*num_entries=*/1));

  // Priority user types should be gathered, but none are returned in this test.
  EXPECT_CALL(sharing_message_contributor_, GetContribution(kMaxEntries - 1));

  // Non-priority user types shouldn't even be gathered.
  EXPECT_CALL(bookmark_contributor_, GetContribution(_)).Times(0);
  EXPECT_CALL(preference_contributor_, GetContribution(_)).Times(0);

  EXPECT_THAT(
      processor_.GatherCommitContributions(/*max_entries=*/kMaxEntries,
                                           /*cookie_jar_mismatch=*/false,
                                           /*cookie_jar_empty=*/false),
      UnorderedElementsAre(Pair(NIGORI, HasNumEntries(1))));
}

TEST_F(CommitProcessorTest, ShouldGatherPriorityUserTypesOnlyContribution) {
  const int kNumReturnedEntries = 3;

  EXPECT_CALL(sharing_message_contributor_, GetContribution(kMaxEntries))
      .WillOnce(ReturnContributionWithEntries(kNumReturnedEntries));

  // Non-priority user types shouldn't even be gathered.
  EXPECT_CALL(bookmark_contributor_, GetContribution(_)).Times(0);
  EXPECT_CALL(preference_contributor_, GetContribution(_)).Times(0);

  EXPECT_THAT(
      processor_.GatherCommitContributions(/*max_entries=*/kMaxEntries,
                                           /*cookie_jar_mismatch=*/false,
                                           /*cookie_jar_empty=*/false),
      UnorderedElementsAre(
          Pair(SHARING_MESSAGE, HasNumEntries(kNumReturnedEntries))));
}

TEST_F(CommitProcessorTest, ShouldGatherRegularUserTypes) {
  const int kNumReturnedBookmarks = 7;

  // High-priority types should be gathered, but no entries are produced.
  EXPECT_CALL(nigori_contributor_, GetContribution(kMaxEntries));
  EXPECT_CALL(sharing_message_contributor_, GetContribution(kMaxEntries));

  // Return |kNumReturnedBookmarks| bookmarks.
  EXPECT_CALL(bookmark_contributor_, GetContribution(kMaxEntries))
      .WillOnce(ReturnContributionWithEntries(kNumReturnedBookmarks));

  // Preferences should also be gathered, but no entries are produced in this
  // test. The precise argument depends on the iteration order so it's not
  // verified in this test.
  EXPECT_CALL(preference_contributor_, GetContribution(_));

  EXPECT_THAT(
      processor_.GatherCommitContributions(/*max_entries=*/kMaxEntries,
                                           /*cookie_jar_mismatch=*/false,
                                           /*cookie_jar_empty=*/false),
      UnorderedElementsAre(
          Pair(BOOKMARKS, HasNumEntries(kNumReturnedBookmarks))));
}

TEST_F(CommitProcessorTest, ShouldGatherMultipleRegularUserTypes) {
  const int kNumReturnedBookmarks = 7;
  const int kNumReturnedPreferences = 8;

  // Return |kNumReturnedBookmarks| bookmarks and |kNumReturnedPreferences|
  // preferences.
  EXPECT_CALL(bookmark_contributor_, GetContribution(_))
      .WillOnce(ReturnContributionWithEntries(kNumReturnedBookmarks));
  EXPECT_CALL(preference_contributor_, GetContribution(_))
      .WillOnce(ReturnContributionWithEntries(kNumReturnedPreferences));

  EXPECT_THAT(
      processor_.GatherCommitContributions(/*max_entries=*/kMaxEntries,
                                           /*cookie_jar_mismatch=*/false,
                                           /*cookie_jar_empty=*/false),
      UnorderedElementsAre(
          Pair(BOOKMARKS, HasNumEntries(kNumReturnedBookmarks)),
          Pair(PREFERENCES, HasNumEntries(kNumReturnedPreferences))));
}

}  // namespace

}  // namespace syncer
