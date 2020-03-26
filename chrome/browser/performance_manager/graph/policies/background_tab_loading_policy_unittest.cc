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

}  // namespace policies

}  // namespace performance_manager
