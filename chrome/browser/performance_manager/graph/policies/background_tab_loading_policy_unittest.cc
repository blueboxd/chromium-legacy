// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/graph/policies/background_tab_loading_policy.h"

#include <vector>

#include "chrome/browser/performance_manager/mechanisms/tab_loader.h"
#include "components/performance_manager/graph/page_node_impl.h"
#include "components/performance_manager/test_support/graph_test_harness.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace performance_manager {

namespace policies {

// Mock version of a performance_manager::mechanism::TabLoader.
class LenientMockTabLoader : public performance_manager::mechanism::TabLoader {
 public:
  LenientMockTabLoader() = default;
  ~LenientMockTabLoader() override = default;
  LenientMockTabLoader(const LenientMockTabLoader& other) = delete;
  LenientMockTabLoader& operator=(const LenientMockTabLoader&) = delete;

  MOCK_METHOD1(LoadPageNode, void(const PageNode* page_node));
};
using MockTabLoader = ::testing::StrictMock<LenientMockTabLoader>;

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

    // Make the policy use a mock TabLoader.
    auto mock_loader = std::make_unique<MockTabLoader>();
    mock_loader_ = mock_loader.get();
    policy_->SetMockLoaderForTesting(std::move(mock_loader));
  }

  void TearDown() override { graph()->TakeFromGraph(policy_); }

 protected:
  BackgroundTabLoadingPolicy* policy() { return policy_; }
  MockTabLoader* loader() { return mock_loader_; }

 private:
  BackgroundTabLoadingPolicy* policy_;
  MockTabLoader* mock_loader_;
};

TEST_F(BackgroundTabLoadingPolicyTest, RestoreTabs) {
  std::vector<
      performance_manager::TestNodeWrapper<performance_manager::PageNodeImpl>>
      page_nodes;
  std::vector<PageNode*> raw_page_nodes;

  // Create vector of PageNode to restore.
  for (int i = 0; i < 4; i++) {
    page_nodes.push_back(CreateNode<performance_manager::PageNodeImpl>());
    raw_page_nodes.push_back(page_nodes.back().get());
    EXPECT_CALL(*loader(), LoadPageNode(raw_page_nodes.back()));
  }

  policy()->RestoreTabs(raw_page_nodes);
}

}  // namespace policies

}  // namespace performance_manager
