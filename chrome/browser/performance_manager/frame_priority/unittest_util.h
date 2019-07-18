// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MANAGER_FRAME_PRIORITY_UNITTEST_UTIL_H_
#define CHROME_BROWSER_PERFORMANCE_MANAGER_FRAME_PRIORITY_UNITTEST_UTIL_H_

#include "chrome/browser/performance_manager/public/frame_priority/frame_priority.h"

#include "base/macros.h"

namespace performance_manager {
namespace frame_priority {
namespace test {

// A dummy consumer that simply maintains a list of all submitted votes and
// doesn't explicitly clean them up. New votes are continuously pushed back to
// the end of |votes_|.
class DummyVoteConsumer : public VoteConsumer {
 public:
  DummyVoteConsumer();
  ~DummyVoteConsumer() override;

  // VoteConsumer implementation:
  VoteReceipt SubmitVote(VoterId voter_id, const Vote& vote) override;
  void VoteInvalidated(AcceptedVote* vote) override;

  // Checks that the vote at position |index| is valid, and has the
  // corresponding |voter|, |frame_node| and |priority|.
  void ExpectValidVote(size_t index,
                       VoterId voter_id,
                       const FrameNode* frame_node,
                       base::TaskPriority priority);

  VotingChannelFactory voting_channel_factory_;
  std::vector<AcceptedVote> votes_;
  size_t valid_vote_count_ = 0;

  DISALLOW_COPY_AND_ASSIGN(DummyVoteConsumer);
};

// A dummy voter that allows emitting votes and tracking their receipts.
class DummyVoter {
 public:
  DummyVoter();
  ~DummyVoter();

  void SetVotingChannel(VotingChannel&& voting_channel);

  // Causes the voter to emit a vote for the given |frame_node| and with the
  // given |priority|. The receipt is pushed back onto |receipts_|.
  void EmitVote(const FrameNode* frame_node,
                base::TaskPriority priority = base::TaskPriority::LOWEST);

  VotingChannel voting_channel_;
  std::vector<VoteReceipt> receipts_;

 private:
  DISALLOW_COPY_AND_ASSIGN(DummyVoter);
};

}  // namespace test
}  // namespace frame_priority
}  // namespace performance_manager

#endif  // CHROME_BROWSER_PERFORMANCE_MANAGER_FRAME_PRIORITY_UNITTEST_UTIL_H_
