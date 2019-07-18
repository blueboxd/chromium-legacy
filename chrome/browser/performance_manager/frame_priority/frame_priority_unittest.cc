// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/public/frame_priority/frame_priority.h"

#include "chrome/browser/performance_manager/frame_priority/unittest_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace performance_manager {
namespace frame_priority {

namespace {

// Some dummy frames.
const FrameNode* kDummyFrame1 = reinterpret_cast<const FrameNode*>(0xDEADBEEF);
const FrameNode* kDummyFrame2 = reinterpret_cast<const FrameNode*>(0xBAADF00D);

void ExpectEntangled(const VoteReceipt& receipt, const AcceptedVote& vote) {
  EXPECT_TRUE(receipt.HasVote(&vote));
  EXPECT_TRUE(vote.HasReceipt(&receipt));
  EXPECT_TRUE(vote.IsValid());
}

void ExpectNotEntangled(const VoteReceipt& receipt, const AcceptedVote& vote) {
  EXPECT_FALSE(receipt.HasVote());
  EXPECT_FALSE(vote.HasReceipt());
  EXPECT_FALSE(vote.IsValid());
}

}  // namespace

TEST(FramePriorityTest, DefaultAcceptedVoteIsInvalid) {
  AcceptedVote vote;
  EXPECT_FALSE(vote.IsValid());
}

TEST(FramePriorityTest, VoteReceiptsWork) {
  test::DummyVoteConsumer consumer;
  test::DummyVoter voter;

  EXPECT_FALSE(voter.voting_channel_.IsValid());
  voter.SetVotingChannel(consumer.voting_channel_factory_.BuildVotingChannel());
  EXPECT_EQ(&consumer.voting_channel_factory_,
            voter.voting_channel_.factory_for_testing());
  EXPECT_NE(kInvalidVoterId, voter.voting_channel_.voter_id());
  EXPECT_TRUE(voter.voting_channel_.IsValid());

  voter.EmitVote(kDummyFrame1);
  EXPECT_EQ(1u, voter.receipts_.size());
  EXPECT_EQ(1u, consumer.votes_.size());
  EXPECT_EQ(1u, consumer.valid_vote_count_);
  EXPECT_EQ(voter.voting_channel_.voter_id(), consumer.votes_[0].voter_id());
  EXPECT_EQ(kDummyFrame1, consumer.votes_[0].vote().frame_node());
  EXPECT_TRUE(consumer.votes_[0].IsValid());
  ExpectEntangled(voter.receipts_[0], consumer.votes_[0]);

  // Move the vote and the receipt out of their containers and back in.
  // All should be well.
  {
    VoteReceipt receipt = std::move(voter.receipts_[0]);
    EXPECT_FALSE(voter.receipts_[0].HasVote());
    ExpectEntangled(receipt, consumer.votes_[0]);

    AcceptedVote vote = std::move(consumer.votes_[0]);
    EXPECT_FALSE(consumer.votes_[0].IsValid());
    ExpectEntangled(receipt, vote);

    voter.receipts_[0] = std::move(receipt);
    EXPECT_FALSE(receipt.HasVote());
    ExpectEntangled(voter.receipts_[0], vote);

    consumer.votes_[0] = std::move(vote);
    EXPECT_FALSE(vote.IsValid());
    ExpectEntangled(voter.receipts_[0], consumer.votes_[0]);
  }

  voter.EmitVote(kDummyFrame2);
  EXPECT_EQ(2u, voter.receipts_.size());
  EXPECT_EQ(2u, consumer.votes_.size());
  EXPECT_EQ(2u, consumer.valid_vote_count_);
  EXPECT_EQ(kDummyFrame1, consumer.votes_[0].vote().frame_node());
  EXPECT_EQ(kDummyFrame2, consumer.votes_[1].vote().frame_node());
  ExpectEntangled(voter.receipts_[0], consumer.votes_[0]);
  ExpectEntangled(voter.receipts_[1], consumer.votes_[1]);

  // Cancel a vote.
  voter.receipts_[0].Reset();
  EXPECT_EQ(2u, voter.receipts_.size());
  EXPECT_EQ(2u, consumer.votes_.size());
  EXPECT_EQ(1u, consumer.valid_vote_count_);
  EXPECT_EQ(kDummyFrame1, consumer.votes_[0].vote().frame_node());
  EXPECT_EQ(kDummyFrame2, consumer.votes_[1].vote().frame_node());
  ExpectNotEntangled(voter.receipts_[0], consumer.votes_[0]);
  ExpectEntangled(voter.receipts_[1], consumer.votes_[1]);

  // Cause the votes to be moved by deleting the invalid one.
  consumer.votes_.erase(consumer.votes_.begin());
  EXPECT_EQ(2u, voter.receipts_.size());
  EXPECT_EQ(1u, consumer.votes_.size());
  EXPECT_EQ(1u, consumer.valid_vote_count_);
  EXPECT_EQ(kDummyFrame2, consumer.votes_[0].vote().frame_node());
  EXPECT_FALSE(voter.receipts_[0].HasVote());
  ExpectEntangled(voter.receipts_[1], consumer.votes_[0]);

  // Cause the receipts to be moved by deleting the empty one.
  voter.receipts_.erase(voter.receipts_.begin());
  EXPECT_EQ(1u, voter.receipts_.size());
  EXPECT_EQ(1u, consumer.votes_.size());
  EXPECT_EQ(1u, consumer.valid_vote_count_);
  EXPECT_EQ(kDummyFrame2, consumer.votes_[0].vote().frame_node());
  ExpectEntangled(voter.receipts_[0], consumer.votes_[0]);

  // Cancel the remaining vote by deleting the receipt.
  voter.receipts_.clear();
  EXPECT_EQ(0u, voter.receipts_.size());
  EXPECT_EQ(1u, consumer.votes_.size());
  EXPECT_EQ(0u, consumer.valid_vote_count_);
  EXPECT_EQ(kDummyFrame2, consumer.votes_[0].vote().frame_node());
  EXPECT_FALSE(consumer.votes_[0].HasReceipt());
  EXPECT_FALSE(consumer.votes_[0].IsValid());
}

}  // namespace frame_priority
}  // namespace performance_manager
