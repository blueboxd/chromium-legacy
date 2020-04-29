// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/policies/urgent_page_discarding_policy.h"

#include <memory>

#include "base/memory/memory_pressure_listener.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/time/time.h"
#include "base/util/memory_pressure/fake_memory_pressure_monitor.h"
#include "chrome/browser/performance_manager/decorators/page_aggregator.h"
#include "chrome/browser/performance_manager/mechanisms/page_discarder.h"
#include "components/performance_manager/graph/frame_node_impl.h"
#include "components/performance_manager/graph/graph_impl.h"
#include "components/performance_manager/graph/node_attached_data_impl.h"
#include "components/performance_manager/graph/page_node_impl.h"
#include "components/performance_manager/graph/process_node_impl.h"
#include "components/performance_manager/public/decorators/page_live_state_decorator.h"
#include "components/performance_manager/test_support/graph_test_harness.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace performance_manager {
namespace policies {

using ::testing::Return;

// Class allowing setting some fake PageLiveStateDecorator::Data for a PageNode.
class FakePageLiveStateData
    : public PageLiveStateDecorator::Data,
      public NodeAttachedDataImpl<FakePageLiveStateData> {
 public:
  struct Traits : public NodeAttachedDataInMap<PageNodeImpl> {};
  ~FakePageLiveStateData() override = default;
  FakePageLiveStateData(const FakePageLiveStateData& other) = delete;
  FakePageLiveStateData& operator=(const FakePageLiveStateData&) = delete;

  // PageLiveStateDecorator::Data:
  bool IsConnectedToUSBDevice() const override {
    return is_connected_to_usb_device_;
  }
  bool IsConnectedToBluetoothDevice() const override {
    return is_connected_to_bluetooth_device_;
  }
  bool IsCapturingVideo() const override { return is_capturing_video_; }
  bool IsCapturingAudio() const override { return is_capturing_audio_; }
  bool IsBeingMirrored() const override { return is_being_mirrored_; }
  bool IsCapturingDesktop() const override { return is_capturing_desktop_; }
  bool IsAutoDiscardable() const override { return is_auto_discardable_; }
  bool WasDiscarded() const override { return was_discarded_; }

  bool is_connected_to_usb_device_ = false;
  bool is_connected_to_bluetooth_device_ = false;
  bool is_capturing_video_ = false;
  bool is_capturing_audio_ = false;
  bool is_being_mirrored_ = false;
  bool is_capturing_desktop_ = false;
  bool is_auto_discardable_ = true;
  bool was_discarded_ = false;

 private:
  friend class ::performance_manager::NodeAttachedDataImpl<
      FakePageLiveStateData>;

  explicit FakePageLiveStateData(const PageNodeImpl* page_node) {}
};

// Mock version of a performance_manager::mechanism::PageDiscarder.
class LenientMockPageDiscarder
    : public performance_manager::mechanism::PageDiscarder {
 public:
  LenientMockPageDiscarder() = default;
  ~LenientMockPageDiscarder() override = default;
  LenientMockPageDiscarder(const LenientMockPageDiscarder& other) = delete;
  LenientMockPageDiscarder& operator=(const LenientMockPageDiscarder&) = delete;

  MOCK_METHOD1(DiscardPageNodeImpl, bool(const PageNode* page_node));

 private:
  void DiscardPageNode(
      const PageNode* page_node,
      base::OnceCallback<void(bool)> post_discard_cb) override {
    std::move(post_discard_cb).Run(DiscardPageNodeImpl(page_node));
  }
};
using MockPageDiscarder = ::testing::StrictMock<LenientMockPageDiscarder>;

// Test version of the UrgentPageDiscardingPolicy.
class TestUrgentPageDiscardingPolicy : public UrgentPageDiscardingPolicy {
 public:
  TestUrgentPageDiscardingPolicy() = default;
  ~TestUrgentPageDiscardingPolicy() override = default;
  TestUrgentPageDiscardingPolicy(const TestUrgentPageDiscardingPolicy& other) =
      delete;
  TestUrgentPageDiscardingPolicy& operator=(
      const TestUrgentPageDiscardingPolicy&) = delete;

 protected:
  const PageLiveStateDecorator::Data* GetPageNodeLiveStateData(
      const PageNode* page_node) const override {
    // Returns a fake version of the PageLiveStateDecorator::Data, create it if
    // it doesn't exist. Tests that want to set some fake live state data should
    // call |FakePageLiveStateData::GetOrCreate|.
    return FakePageLiveStateData::GetOrCreate(
        PageNodeImpl::FromNode(page_node));
  }
};

class UrgentPageDiscardingPolicyTest : public GraphTestHarness {
 public:
  UrgentPageDiscardingPolicyTest() {
    // Some tests depends on the existence of the PageAggregator.
    graph()->PassToGraph(std::make_unique<PageAggregator>());
  }
  ~UrgentPageDiscardingPolicyTest() override = default;
  UrgentPageDiscardingPolicyTest(const UrgentPageDiscardingPolicyTest& other) =
      delete;
  UrgentPageDiscardingPolicyTest& operator=(
      const UrgentPageDiscardingPolicyTest&) = delete;

  void SetUp() override {
    // Create the policy and pass it to the graph.
    auto policy = std::make_unique<TestUrgentPageDiscardingPolicy>();
    policy_ = policy.get();
    graph()->PassToGraph(std::move(policy));

    // Make the policy use a mock PageDiscarder.
    auto mock_discarder = std::make_unique<MockPageDiscarder>();
    mock_discarder_ = mock_discarder.get();
    policy_->SetMockDiscarderForTesting(std::move(mock_discarder));

    // Create a PageNode and make it discardable.
    process_node_ = CreateNode<performance_manager::ProcessNodeImpl>();
    page_node_ = CreateNode<performance_manager::PageNodeImpl>();
    main_frame_node_ =
        CreateFrameNodeAutoId(process_node_.get(), page_node_.get());
    main_frame_node_->SetIsCurrent(true);
    MakePageNodeDiscardable(page_node());

    histogram_tester_ = std::make_unique<base::HistogramTester>();
  }

  void TearDown() override {
    graph()->TakeFromGraph(policy_);
    main_frame_node_.reset();
    page_node_.reset();
    process_node_.reset();
    histogram_tester_.reset();
  }

 protected:
  void SimulateMemoryPressure(size_t pressure_event_counts = 1) {
    for (size_t i = 0; i < pressure_event_counts; ++i) {
      mem_pressure_monitor_.SetAndNotifyMemoryPressure(
          base::MemoryPressureListener::MemoryPressureLevel::
              MEMORY_PRESSURE_LEVEL_CRITICAL);
      task_env().RunUntilIdle();
    }
    mem_pressure_monitor_.SetAndNotifyMemoryPressure(
        base::MemoryPressureListener::MemoryPressureLevel::
            MEMORY_PRESSURE_LEVEL_MODERATE);
    task_env().RunUntilIdle();
  }

  // Make sure that |page_node| is discardable.
  void MakePageNodeDiscardable(PageNodeImpl* page_node);

  TestUrgentPageDiscardingPolicy* policy() { return policy_; }
  PageNodeImpl* page_node() { return page_node_.get(); }
  ProcessNodeImpl* process_node() { return process_node_.get(); }
  FrameNodeImpl* frame_node() { return main_frame_node_.get(); }
  void ResetFrameNode() { main_frame_node_.reset(); }
  MockPageDiscarder* discarder() { return mock_discarder_; }
  util::test::FakeMemoryPressureMonitor* mem_pressure_monitor() {
    return &mem_pressure_monitor_;
  }
  base::HistogramTester* histogram_tester() { return histogram_tester_.get(); }

 private:
  util::test::FakeMemoryPressureMonitor mem_pressure_monitor_;
  TestUrgentPageDiscardingPolicy* policy_;
  MockPageDiscarder* mock_discarder_;
  performance_manager::TestNodeWrapper<performance_manager::PageNodeImpl>
      page_node_;
  performance_manager::TestNodeWrapper<performance_manager::ProcessNodeImpl>
      process_node_;
  performance_manager::TestNodeWrapper<performance_manager::FrameNodeImpl>
      main_frame_node_;
  std::unique_ptr<base::HistogramTester> histogram_tester_;
};

void UrgentPageDiscardingPolicyTest::MakePageNodeDiscardable(
    PageNodeImpl* page_node) {
  page_node->SetIsVisible(false);
  page_node->SetIsAudible(false);
  const auto kUrl = GURL("https://foo.com");
  page_node->OnMainFrameNavigationCommitted(false, base::TimeTicks::Now(), 42,
                                            kUrl, "text/html");
  (*page_node->main_frame_nodes().begin())->OnNavigationCommitted(kUrl, false);
  AdvanceClock(base::TimeDelta::FromMinutes(10));
  EXPECT_TRUE(policy()->CanUrgentlyDiscardForTesting(page_node));
}

TEST_F(UrgentPageDiscardingPolicyTest, TestCannotDiscardVisiblePage) {
  page_node()->SetIsVisible(true);
  EXPECT_FALSE(policy()->CanUrgentlyDiscardForTesting(page_node()));
}

TEST_F(UrgentPageDiscardingPolicyTest, TestCannotDiscardAudiblePage) {
  page_node()->SetIsAudible(true);
  EXPECT_FALSE(policy()->CanUrgentlyDiscardForTesting(page_node()));
}

TEST_F(UrgentPageDiscardingPolicyTest,
       TestCannotDiscardPageWithDiscardAttemptMarker) {
  policy()->AdornsPageWithDiscardAttemptMarkerForTesting(page_node());
  EXPECT_FALSE(policy()->CanUrgentlyDiscardForTesting(page_node()));
}

TEST_F(UrgentPageDiscardingPolicyTest, TestCannotDiscardRecentlyAudiblePage) {
  page_node()->SetIsAudible(true);
  page_node()->SetIsAudible(false);
  EXPECT_FALSE(policy()->CanUrgentlyDiscardForTesting(page_node()));
}

#if !defined(OS_CHROMEOS)
TEST_F(UrgentPageDiscardingPolicyTest, TestCannotDiscardRecentlyVisiblePage) {
  page_node()->SetIsVisible(true);
  page_node()->SetIsVisible(false);
  EXPECT_FALSE(policy()->CanUrgentlyDiscardForTesting(page_node()));
}
#endif

TEST_F(UrgentPageDiscardingPolicyTest, TestCannotDiscardPdf) {
  page_node()->OnMainFrameNavigationCommitted(false, base::TimeTicks::Now(), 53,
                                              GURL("https://foo.com/doc.pdf"),
                                              "application/pdf");
  EXPECT_FALSE(policy()->CanUrgentlyDiscardForTesting(page_node()));
}

TEST_F(UrgentPageDiscardingPolicyTest, TestCannotDiscardPageWithoutMainFrame) {
  ResetFrameNode();
  EXPECT_FALSE(policy()->CanUrgentlyDiscardForTesting(page_node()));
}

TEST_F(UrgentPageDiscardingPolicyTest, TestCannotDiscardExtension) {
  frame_node()->OnNavigationCommitted(GURL("chrome-extention://foo"), false);
  EXPECT_FALSE(policy()->CanUrgentlyDiscardForTesting(page_node()));
}

TEST_F(UrgentPageDiscardingPolicyTest, TestCannotDiscardPageWithInvalidURL) {
  frame_node()->OnNavigationCommitted(GURL("foo42"), false);
  EXPECT_FALSE(policy()->CanUrgentlyDiscardForTesting(page_node()));
}

TEST_F(UrgentPageDiscardingPolicyTest,
       TestCannotDiscardPageProtectedByExtension) {
  FakePageLiveStateData::GetOrCreate(page_node())->is_auto_discardable_ = false;
  EXPECT_FALSE(policy()->CanUrgentlyDiscardForTesting(page_node()));
}

TEST_F(UrgentPageDiscardingPolicyTest, TestCannotDiscardPageCapturingVideo) {
  FakePageLiveStateData::GetOrCreate(page_node())->is_capturing_video_ = true;
  EXPECT_FALSE(policy()->CanUrgentlyDiscardForTesting(page_node()));
}

TEST_F(UrgentPageDiscardingPolicyTest, TestCannotDiscardPageCapturingAudio) {
  FakePageLiveStateData::GetOrCreate(page_node())->is_capturing_audio_ = true;
  EXPECT_FALSE(policy()->CanUrgentlyDiscardForTesting(page_node()));
}

TEST_F(UrgentPageDiscardingPolicyTest, TestCannotDiscardPageBeingMirrored) {
  FakePageLiveStateData::GetOrCreate(page_node())->is_being_mirrored_ = true;
  EXPECT_FALSE(policy()->CanUrgentlyDiscardForTesting(page_node()));
}

TEST_F(UrgentPageDiscardingPolicyTest, TestCannotDiscardPageCapturingDesktop) {
  FakePageLiveStateData::GetOrCreate(page_node())->is_capturing_desktop_ = true;
  EXPECT_FALSE(policy()->CanUrgentlyDiscardForTesting(page_node()));
}

TEST_F(UrgentPageDiscardingPolicyTest,
       TestCannotDiscardPageConnectedToBluetoothDevice) {
  FakePageLiveStateData::GetOrCreate(page_node())
      ->is_connected_to_bluetooth_device_ = true;
  EXPECT_FALSE(policy()->CanUrgentlyDiscardForTesting(page_node()));
}

TEST_F(UrgentPageDiscardingPolicyTest,
       TestCannotDiscardIsConnectedToUSBDevice) {
  FakePageLiveStateData::GetOrCreate(page_node())->is_connected_to_usb_device_ =
      true;
  EXPECT_FALSE(policy()->CanUrgentlyDiscardForTesting(page_node()));
}

#if !defined(OS_CHROMEOS)
TEST_F(UrgentPageDiscardingPolicyTest, TestCannotDiscardPageMultipleTimes) {
  FakePageLiveStateData::GetOrCreate(page_node())->was_discarded_ = true;
  EXPECT_FALSE(policy()->CanUrgentlyDiscardForTesting(page_node()));
}
#endif

TEST_F(UrgentPageDiscardingPolicyTest,
       TestCannotDiscardPageWithFormInteractions) {
  frame_node()->SetHadFormInteraction();
  EXPECT_FALSE(policy()->CanUrgentlyDiscardForTesting(page_node()));
}

TEST_F(UrgentPageDiscardingPolicyTest, UrgentlyDiscardAPageNoCandidate) {
  page_node()->SetIsVisible(true);
  SimulateMemoryPressure();
  testing::Mock::VerifyAndClearExpectations(discarder());
  histogram_tester()->ExpectBucketCount("Discarding.DiscardCandidatesCount", 0,
                                        1);
}

TEST_F(UrgentPageDiscardingPolicyTest, UrgentlyDiscardAPageSingleCandidate) {
  EXPECT_CALL(*discarder(), DiscardPageNodeImpl(page_node()))
      .WillOnce(Return(true));
  SimulateMemoryPressure();
  testing::Mock::VerifyAndClearExpectations(discarder());
  histogram_tester()->ExpectBucketCount("Discarding.DiscardCandidatesCount", 1,
                                        1);
}

TEST_F(UrgentPageDiscardingPolicyTest,
       UrgentlyDiscardAPageSingleCandidateFails) {
  EXPECT_CALL(*discarder(), DiscardPageNodeImpl(page_node()))
      .WillOnce(Return(false));
  SimulateMemoryPressure();
  testing::Mock::VerifyAndClearExpectations(discarder());
  // There should be 2 discard attempts, during the first one an attempt will be
  // made to discard |page_node()|, on the second attempt no discard candidate
  // should be found.
  histogram_tester()->ExpectBucketCount("Discarding.DiscardCandidatesCount", 1,
                                        1);

  histogram_tester()->ExpectBucketCount("Discarding.DiscardCandidatesCount", 0,
                                        1);
}

TEST_F(UrgentPageDiscardingPolicyTest, UrgentlyDiscardAPageTwoCandidates) {
  auto process_node2 = CreateNode<performance_manager::ProcessNodeImpl>();
  auto page_node2 = CreateNode<performance_manager::PageNodeImpl>();
  auto main_frame_node2 =
      CreateFrameNodeAutoId(process_node2.get(), page_node2.get());
  main_frame_node2->SetIsCurrent(true);
  MakePageNodeDiscardable(page_node2.get());

  // Pretend that |page_node2| is the most recently visible page.
  page_node2->SetIsVisible(true);
  AdvanceClock(base::TimeDelta::FromMinutes(30));
  page_node2->SetIsVisible(false);
  AdvanceClock(base::TimeDelta::FromMinutes(30));
  EXPECT_TRUE(policy()->CanUrgentlyDiscardForTesting(page_node2.get()));
  EXPECT_GT(page_node()->TimeSinceLastVisibilityChange(),
            page_node2->TimeSinceLastVisibilityChange());

  process_node()->set_resident_set_kb(1024);
  process_node2->set_resident_set_kb(2048);

  // |page_node2| should be discarded despite being the most recently visible
  // page as it has a bigger footprint.
  EXPECT_CALL(*discarder(), DiscardPageNodeImpl(page_node2.get()))
      .WillOnce(Return(true));
  SimulateMemoryPressure();
  testing::Mock::VerifyAndClearExpectations(discarder());
  histogram_tester()->ExpectBucketCount("Discarding.DiscardCandidatesCount", 2,
                                        1);
  histogram_tester()->ExpectUniqueSample("Discarding.LargestTabFootprint",
                                         2048 / 1024, 1);
  histogram_tester()->ExpectUniqueSample("Discarding.OldestTabFootprint",
                                         1024 / 1024, 1);
}

TEST_F(UrgentPageDiscardingPolicyTest,
       UrgentlyDiscardAPageTwoCandidatesFirstFails) {
  auto process_node2 = CreateNode<performance_manager::ProcessNodeImpl>();
  auto page_node2 = CreateNode<performance_manager::PageNodeImpl>();
  auto main_frame_node2 =
      CreateFrameNodeAutoId(process_node2.get(), page_node2.get());
  main_frame_node2->SetIsCurrent(true);
  MakePageNodeDiscardable(page_node2.get());

  process_node()->set_resident_set_kb(1024);
  process_node2->set_resident_set_kb(2048);

  // Pretends that the first discardable page hasn't been discarded
  // successfully, the other one should be discarded in this case.
  EXPECT_CALL(*discarder(), DiscardPageNodeImpl(page_node2.get()))
      .WillOnce(Return(false));
  EXPECT_CALL(*discarder(), DiscardPageNodeImpl(page_node()))
      .WillOnce(Return(true));
  SimulateMemoryPressure();
  testing::Mock::VerifyAndClearExpectations(discarder());
}

TEST_F(UrgentPageDiscardingPolicyTest,
       UrgentlyDiscardAPageTwoCandidatesMultipleFrames) {
  auto process_node2 = CreateNode<performance_manager::ProcessNodeImpl>();
  auto page_node2 = CreateNode<performance_manager::PageNodeImpl>();
  auto main_frame_node2 =
      CreateFrameNodeAutoId(process_node2.get(), page_node2.get());
  main_frame_node2->SetIsCurrent(true);
  MakePageNodeDiscardable(page_node2.get());
  // Adds a second frame to |page_node()| and host it in |process_node2|.
  auto page_node1_extra_frame =
      CreateFrameNodeAutoId(process_node2.get(), page_node(), frame_node());

  process_node()->set_resident_set_kb(1024);
  process_node2->set_resident_set_kb(2048);

  // The total RSS of |page_node()| should be 1024 + 2048 / 2 = 2048 and the
  // RSS of |page_node2| should be 2048 / 2 = 1024, so |page_node()| will get
  // discarded despite having spent less time in background than |page_node2|.
  EXPECT_CALL(*discarder(), DiscardPageNodeImpl(page_node()))
      .WillOnce(Return(true));
  SimulateMemoryPressure();
  testing::Mock::VerifyAndClearExpectations(discarder());
}

TEST_F(UrgentPageDiscardingPolicyTest,
       UrgentlyDiscardAPageTwoCandidatesNoRSSData) {
  auto process_node2 = CreateNode<performance_manager::ProcessNodeImpl>();
  auto page_node2 = CreateNode<performance_manager::PageNodeImpl>();
  auto main_frame_node2 =
      CreateFrameNodeAutoId(process_node2.get(), page_node2.get());
  main_frame_node2->SetIsCurrent(true);
  MakePageNodeDiscardable(page_node2.get());

  // Pretend that |page_node()| is the most recently visible page.
  page_node()->SetIsVisible(true);
  AdvanceClock(base::TimeDelta::FromMinutes(30));
  page_node()->SetIsVisible(false);
  AdvanceClock(base::TimeDelta::FromMinutes(30));
  EXPECT_TRUE(policy()->CanUrgentlyDiscardForTesting(page_node()));
  EXPECT_GT(page_node2->TimeSinceLastVisibilityChange(),
            page_node()->TimeSinceLastVisibilityChange());

  // |page_node2| should be discarded as there's no RSS data for any of the
  // pages and it's the least recently visible page.
  EXPECT_CALL(*discarder(), DiscardPageNodeImpl(page_node2.get()))
      .WillOnce(Return(true));
  SimulateMemoryPressure();
  testing::Mock::VerifyAndClearExpectations(discarder());
}

TEST_F(UrgentPageDiscardingPolicyTest, NoDiscardOnModeratePressure) {
  // No tab should be discarded on moderate pressure.
  mem_pressure_monitor()->SetAndNotifyMemoryPressure(
      base::MemoryPressureListener::MemoryPressureLevel::
          MEMORY_PRESSURE_LEVEL_MODERATE);
  task_env().RunUntilIdle();
  testing::Mock::VerifyAndClearExpectations(discarder());
}

}  // namespace policies
}  // namespace performance_manager
