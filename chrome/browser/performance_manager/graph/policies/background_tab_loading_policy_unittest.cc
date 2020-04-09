// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/graph/policies/background_tab_loading_policy.h"

#include <vector>

#include "chrome/browser/performance_manager/mechanisms/page_loader.h"
#include "components/performance_manager/graph/page_node_impl.h"
#include "components/performance_manager/public/decorators/tab_properties_decorator.h"
#include "components/performance_manager/test_support/graph_test_harness.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace performance_manager {

namespace policies {

// Mock version of a performance_manager::mechanism::PageLoader.
class LenientMockPageLoader
    : public performance_manager::mechanism::PageLoader {
 public:
  LenientMockPageLoader() = default;
  ~LenientMockPageLoader() override = default;
  LenientMockPageLoader(const LenientMockPageLoader& other) = delete;
  LenientMockPageLoader& operator=(const LenientMockPageLoader&) = delete;

  MOCK_METHOD1(LoadPageNode, void(const PageNode* page_node));
};
using MockPageLoader = ::testing::StrictMock<LenientMockPageLoader>;

class BackgroundTabLoadingPolicyTest : public GraphTestHarness {
 public:
  BackgroundTabLoadingPolicyTest() = default;
  ~BackgroundTabLoadingPolicyTest() override = default;
  BackgroundTabLoadingPolicyTest(const BackgroundTabLoadingPolicyTest& other) =
      delete;
  BackgroundTabLoadingPolicyTest& operator=(
      const BackgroundTabLoadingPolicyTest&) = delete;

  void SetUp() override {
    // Create the policy.
    auto policy = std::make_unique<BackgroundTabLoadingPolicy>();
    policy_ = policy.get();
    graph()->PassToGraph(std::move(policy));

    // Make the policy use a mock PageLoader.
    auto mock_loader = std::make_unique<MockPageLoader>();
    mock_loader_ = mock_loader.get();
    policy_->SetMockLoaderForTesting(std::move(mock_loader));

    // Set a value explicitly for MaxSimultaneousLoad threshold to avoid a
    // dependency on the number of cores of the machine on which the test runs.
    policy_->SetMaxSimultaneousLoadsForTesting(4);
    policy_->SetFreeMemoryForTesting(150);
  }

  void TearDown() override { graph()->TakeFromGraph(policy_); }

 protected:
  BackgroundTabLoadingPolicy* policy() { return policy_; }
  MockPageLoader* loader() { return mock_loader_; }

 private:
  BackgroundTabLoadingPolicy* policy_;
  MockPageLoader* mock_loader_;
};

TEST_F(BackgroundTabLoadingPolicyTest, ScheduleLoadForRestoredTabs) {
  std::vector<
      performance_manager::TestNodeWrapper<performance_manager::PageNodeImpl>>
      page_nodes;
  std::vector<PageNode*> raw_page_nodes;

  // Create vector of PageNode to restore.
  for (int i = 0; i < 4; i++) {
    page_nodes.push_back(CreateNode<performance_manager::PageNodeImpl>());
    raw_page_nodes.push_back(page_nodes.back().get());
    EXPECT_CALL(*loader(), LoadPageNode(raw_page_nodes.back()));

    // Set |is_tab| property as this is a requirement to pass the PageNode to
    // ScheduleLoadForRestoredTabs().
    TabPropertiesDecorator::SetIsTabForTesting(raw_page_nodes.back(), true);
  }

  policy()->ScheduleLoadForRestoredTabs(raw_page_nodes);
}

TEST_F(BackgroundTabLoadingPolicyTest, AllLoadingSlotsUsed) {
  // Create 4 PageNode to restore.
  std::vector<
      performance_manager::TestNodeWrapper<performance_manager::PageNodeImpl>>
      page_nodes;
  std::vector<PageNode*> raw_page_nodes;

  // Create vector of PageNode to restore.
  for (int i = 0; i < 4; i++) {
    page_nodes.push_back(CreateNode<performance_manager::PageNodeImpl>());
    raw_page_nodes.push_back(page_nodes.back().get());

    // Set |is_tab| property as this is a requirement to pass the PageNode to
    // ScheduleLoadForRestoredTabs().
    TabPropertiesDecorator::SetIsTabForTesting(raw_page_nodes.back(), true);
  }
  PageNodeImpl* page_node_impl = page_nodes[0].get();

  EXPECT_CALL(*loader(), LoadPageNode(raw_page_nodes[0]));
  EXPECT_CALL(*loader(), LoadPageNode(raw_page_nodes[1]));

  // Use 2 loading slots, which means only 2 of the PageNodes should immediately
  // be scheduled to load.
  policy()->SetMaxSimultaneousLoadsForTesting(2);

  policy()->ScheduleLoadForRestoredTabs(raw_page_nodes);
  testing::Mock::VerifyAndClear(loader());

  // Simulate load start of a PageNode that initiated load.
  page_node_impl->SetIsLoading(true);

  // The policy should allow one more PageNode to load after a PageNode finishes
  // loading.
  EXPECT_CALL(*loader(), LoadPageNode(raw_page_nodes[2]));

  // Simulate load finish of a PageNode.
  page_node_impl->SetIsLoading(false);
}

TEST_F(BackgroundTabLoadingPolicyTest, ShouldLoad_MaxTabsToRestore) {
  // Create vector of PageNodes to restore.
  std::vector<
      performance_manager::TestNodeWrapper<performance_manager::PageNodeImpl>>
      page_nodes;
  std::vector<PageNode*> raw_page_nodes;

  for (uint32_t i = 0; i < policy()->kMaxTabsToLoad + 1; i++) {
    page_nodes.push_back(CreateNode<performance_manager::PageNodeImpl>());
    raw_page_nodes.push_back(page_nodes.back().get());
  }

  // Test the maximum number of tabs to load threshold.
  for (uint32_t i = 0; i < policy()->kMaxTabsToLoad; i++) {
    EXPECT_TRUE(policy()->ShouldLoad(raw_page_nodes[i]));
    policy()->tab_loads_started_++;
  }
  EXPECT_FALSE(policy()->ShouldLoad(raw_page_nodes[policy()->kMaxTabsToLoad]));
}

TEST_F(BackgroundTabLoadingPolicyTest, ShouldLoad_MinTabsToRestore) {
  // Create vector of PageNodes to restore.
  std::vector<
      performance_manager::TestNodeWrapper<performance_manager::PageNodeImpl>>
      page_nodes;
  std::vector<PageNode*> raw_page_nodes;

  for (uint32_t i = 0; i < policy()->kMinTabsToLoad + 1; i++) {
    page_nodes.push_back(CreateNode<performance_manager::PageNodeImpl>());
    raw_page_nodes.push_back(page_nodes.back().get());
  }

  // When free memory limit is reached.
  const size_t kFreeMemoryLimit = policy()->kDesiredAmountOfFreeMemoryMb - 1;
  policy()->SetFreeMemoryForTesting(kFreeMemoryLimit);

  // Test that the minimum number of tabs to load is respected.
  for (uint32_t i = 0; i < policy()->kMinTabsToLoad; i++) {
    EXPECT_TRUE(policy()->ShouldLoad(raw_page_nodes[i]));
    policy()->tab_loads_started_++;
  }
  EXPECT_FALSE(policy()->ShouldLoad(raw_page_nodes[policy()->kMinTabsToLoad]));
}

TEST_F(BackgroundTabLoadingPolicyTest, ShouldLoad_FreeMemory) {
  // Create a PageNode to restore.
  performance_manager::TestNodeWrapper<performance_manager::PageNodeImpl>
      page_node;
  PageNode* raw_page_node;

  page_node = CreateNode<performance_manager::PageNodeImpl>();
  raw_page_node = page_node.get();

  // Simulate that kMinTabsToLoad have loaded.
  policy()->tab_loads_started_ = policy()->kMinTabsToLoad;

  // Test the free memory constraint.
  const size_t kFreeMemoryLimit = policy()->kDesiredAmountOfFreeMemoryMb;
  policy()->SetFreeMemoryForTesting(kFreeMemoryLimit);
  EXPECT_TRUE(policy()->ShouldLoad(raw_page_node));
  policy()->SetFreeMemoryForTesting(kFreeMemoryLimit - 1);
  EXPECT_FALSE(policy()->ShouldLoad(raw_page_node));
  policy()->SetFreeMemoryForTesting(kFreeMemoryLimit + 1);
  EXPECT_TRUE(policy()->ShouldLoad(raw_page_node));
}

TEST_F(BackgroundTabLoadingPolicyTest, ShouldLoad_OldTab) {
  // Create an old tab.
  performance_manager::TestNodeWrapper<performance_manager::PageNodeImpl>
      page_node;
  PageNode* raw_page_node;

  page_node = CreateNode<performance_manager::PageNodeImpl>(
      WebContentsProxy(), std::string(), GURL(), false, false,
      base::TimeTicks::Now() - (base::TimeDelta::FromSeconds(1) +
                                policy()->kMaxTimeSinceLastUseToLoad));
  raw_page_node = page_node.get();

  // Simulate that kMinTabsToLoad have loaded.
  policy()->tab_loads_started_ = policy()->kMinTabsToLoad;

  // Test the max time since last use threshold.
  EXPECT_FALSE(policy()->ShouldLoad(raw_page_node));
}

}  // namespace policies

}  // namespace performance_manager
