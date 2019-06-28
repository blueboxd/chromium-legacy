// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/graph/frame_node_impl.h"

#include "base/test/simple_test_tick_clock.h"
#include "chrome/browser/performance_manager/graph/graph_test_harness.h"
#include "chrome/browser/performance_manager/graph/mock_graphs.h"
#include "chrome/browser/performance_manager/graph/page_node_impl.h"
#include "chrome/browser/performance_manager/graph/process_node_impl.h"
#include "chrome/browser/performance_manager/performance_manager_clock.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace performance_manager {

namespace {

class FrameNodeImplTest : public GraphTestHarness {
 public:
  void SetUp() override {
    PerformanceManagerClock::SetClockForTesting(&clock_);

    // Sets a valid starting time.
    clock_.SetNowTicks(base::TimeTicks::Now());
  }

  void TearDown() override { PerformanceManagerClock::ResetClockForTesting(); }

 protected:
  void AdvanceClock(base::TimeDelta delta) { clock_.Advance(delta); }

 private:
  base::SimpleTestTickClock clock_;
};

}  // namespace

TEST_F(FrameNodeImplTest, SafeDowncast) {
  auto process = CreateNode<ProcessNodeImpl>();
  auto page = CreateNode<PageNodeImpl>();
  auto frame = CreateNode<FrameNodeImpl>(process.get(), page.get());
  FrameNode* node = frame.get();
  EXPECT_EQ(frame.get(), FrameNodeImpl::FromNode(node));
  NodeBase* base = frame.get();
  EXPECT_EQ(base, NodeBase::FromNode(node));
  EXPECT_EQ(static_cast<Node*>(node), base->ToNode());
}

TEST_F(FrameNodeImplTest, AddFrameHierarchyBasic) {
  auto process = CreateNode<ProcessNodeImpl>();
  auto page = CreateNode<PageNodeImpl>();
  auto parent_node = CreateNode<FrameNodeImpl>(process.get(), page.get());
  auto child2_node = CreateNode<FrameNodeImpl>(process.get(), page.get(),
                                               parent_node.get(), 1);
  auto child3_node = CreateNode<FrameNodeImpl>(process.get(), page.get(),
                                               parent_node.get(), 2);

  EXPECT_EQ(nullptr, parent_node->parent_frame_node());
  EXPECT_EQ(2u, parent_node->child_frame_nodes().size());
  EXPECT_EQ(parent_node.get(), child2_node->parent_frame_node());
  EXPECT_EQ(parent_node.get(), child3_node->parent_frame_node());
}

TEST_F(FrameNodeImplTest, NavigationCommitted_SameDocument) {
  auto process = CreateNode<ProcessNodeImpl>();
  auto page = CreateNode<PageNodeImpl>();
  auto frame_node = CreateNode<FrameNodeImpl>(process.get(), page.get());
  EXPECT_TRUE(frame_node->url().is_empty());
  const GURL url("http://www.foo.com/");
  frame_node->OnNavigationCommitted(url, /* same_document */ true);
  EXPECT_EQ(url, frame_node->url());
}

TEST_F(FrameNodeImplTest, NavigationCommitted_DifferentDocument) {
  auto process = CreateNode<ProcessNodeImpl>();
  auto page = CreateNode<PageNodeImpl>();
  auto frame_node = CreateNode<FrameNodeImpl>(process.get(), page.get());
  EXPECT_TRUE(frame_node->url().is_empty());
  const GURL url("http://www.foo.com/");
  frame_node->OnNavigationCommitted(url, /* same_document */ false);
  EXPECT_EQ(url, frame_node->url());
}

TEST_F(FrameNodeImplTest, RemoveChildFrame) {
  auto process = CreateNode<ProcessNodeImpl>();
  auto page = CreateNode<PageNodeImpl>();
  auto parent_frame_node = CreateNode<FrameNodeImpl>(process.get(), page.get());
  auto child_frame_node = CreateNode<FrameNodeImpl>(process.get(), page.get(),
                                                    parent_frame_node.get(), 1);

  // Ensure correct Parent-child relationships have been established.
  EXPECT_EQ(1u, parent_frame_node->child_frame_nodes().size());
  EXPECT_TRUE(!parent_frame_node->parent_frame_node());
  EXPECT_EQ(0u, child_frame_node->child_frame_nodes().size());
  EXPECT_EQ(parent_frame_node.get(), child_frame_node->parent_frame_node());

  child_frame_node.reset();

  // Parent-child relationships should no longer exist.
  EXPECT_EQ(0u, parent_frame_node->child_frame_nodes().size());
  EXPECT_TRUE(!parent_frame_node->parent_frame_node());
}

TEST_F(FrameNodeImplTest, IsAdFrame) {
  auto process = CreateNode<ProcessNodeImpl>();
  auto page = CreateNode<PageNodeImpl>();
  auto frame_node = CreateNode<FrameNodeImpl>(process.get(), page.get());
  EXPECT_FALSE(frame_node->is_ad_frame());
  frame_node->SetIsAdFrame();
  EXPECT_TRUE(frame_node->is_ad_frame());
  frame_node->SetIsAdFrame();
  EXPECT_TRUE(frame_node->is_ad_frame());
}

namespace {

class LenientMockObserver : public FrameNodeImpl::Observer {
 public:
  LenientMockObserver() {}
  ~LenientMockObserver() override {}

  MOCK_METHOD1(OnFrameNodeAdded, void(const FrameNode*));
  MOCK_METHOD1(OnBeforeFrameNodeRemoved, void(const FrameNode*));
  MOCK_METHOD1(OnIsCurrentChanged, void(const FrameNode*));
  MOCK_METHOD1(OnNetworkAlmostIdleChanged, void(const FrameNode*));
  MOCK_METHOD1(OnFrameLifecycleStateChanged, void(const FrameNode*));
  MOCK_METHOD1(OnNonPersistentNotificationCreated, void(const FrameNode*));
  MOCK_METHOD1(OnURLChanged, void(const FrameNode*));

  void SetNotifiedFrameNode(const FrameNode* frame_node) {
    notified_frame_node_ = frame_node;
  }

  const FrameNode* TakeNotifiedFrameNode() {
    const FrameNode* node = notified_frame_node_;
    notified_frame_node_ = nullptr;
    return node;
  }

 private:
  const FrameNode* notified_frame_node_ = nullptr;
};

using MockObserver = ::testing::StrictMock<LenientMockObserver>;

using testing::_;
using testing::Invoke;

}  // namespace

TEST_F(FrameNodeImplTest, ObserverWorks) {
  auto process = CreateNode<ProcessNodeImpl>();
  auto page = CreateNode<PageNodeImpl>();

  MockObserver obs;
  graph()->AddFrameNodeObserver(&obs);

  // Create a frame node and expect a matching call to "OnFrameNodeAdded".
  EXPECT_CALL(obs, OnFrameNodeAdded(_))
      .WillOnce(Invoke(&obs, &MockObserver::SetNotifiedFrameNode));
  auto frame_node = CreateNode<FrameNodeImpl>(process.get(), page.get());
  const FrameNode* raw_frame_node = frame_node.get();
  EXPECT_EQ(raw_frame_node, obs.TakeNotifiedFrameNode());

  // Invoke "SetIsCurrent" and expect a "OnIsCurrentChanged" callback.
  EXPECT_CALL(obs, OnIsCurrentChanged(_))
      .WillOnce(Invoke(&obs, &MockObserver::SetNotifiedFrameNode));
  frame_node->SetIsCurrent(true);
  EXPECT_EQ(raw_frame_node, obs.TakeNotifiedFrameNode());

  // Invoke "SetNetworkAlmostIdle" and expect an "OnNetworkAlmostIdleChanged"
  // callback.
  EXPECT_CALL(obs, OnNetworkAlmostIdleChanged(_))
      .WillOnce(Invoke(&obs, &MockObserver::SetNotifiedFrameNode));
  frame_node->SetNetworkAlmostIdle();
  EXPECT_EQ(raw_frame_node, obs.TakeNotifiedFrameNode());

  // Invoke "SetLifecycleState" and expect an "OnFrameLifecycleStateChanged"
  // callback.
  EXPECT_CALL(obs, OnFrameLifecycleStateChanged(_))
      .WillOnce(Invoke(&obs, &MockObserver::SetNotifiedFrameNode));
  frame_node->SetLifecycleState(
      resource_coordinator::mojom::LifecycleState::kFrozen);
  EXPECT_EQ(raw_frame_node, obs.TakeNotifiedFrameNode());

  // Invoke "OnNonPersistentNotificationCreated" and expect an
  // "OnNonPersistentNotificationCreated" callback.
  EXPECT_CALL(obs, OnNonPersistentNotificationCreated(_))
      .WillOnce(Invoke(&obs, &MockObserver::SetNotifiedFrameNode));
  frame_node->OnNonPersistentNotificationCreated();
  EXPECT_EQ(raw_frame_node, obs.TakeNotifiedFrameNode());

  // Invoke "OnNavigationCommitted" and expect an "OnURLChanged" callback.
  EXPECT_CALL(obs, OnURLChanged(_))
      .WillOnce(Invoke(&obs, &MockObserver::SetNotifiedFrameNode));
  frame_node->OnNavigationCommitted(GURL("https://foo.com/"), true);
  EXPECT_EQ(raw_frame_node, obs.TakeNotifiedFrameNode());

  // Release the frame node and expect a call to "OnBeforeFrameNodeRemoved".
  EXPECT_CALL(obs, OnBeforeFrameNodeRemoved(_))
      .WillOnce(Invoke(&obs, &MockObserver::SetNotifiedFrameNode));
  frame_node.reset();
  EXPECT_EQ(raw_frame_node, obs.TakeNotifiedFrameNode());

  graph()->RemoveFrameNodeObserver(&obs);
}

TEST_F(FrameNodeImplTest, PublicInterface) {
  auto process = CreateNode<ProcessNodeImpl>();
  auto page = CreateNode<PageNodeImpl>();
  auto frame_node = CreateNode<FrameNodeImpl>(process.get(), page.get());
  const FrameNode* public_frame_node = frame_node.get();

  // Simply test that the public interface impls yield the same result as their
  // private counterpart.

  EXPECT_EQ(static_cast<const FrameNode*>(frame_node->parent_frame_node()),
            public_frame_node->GetParentFrameNode());
  EXPECT_EQ(static_cast<const PageNode*>(frame_node->page_node()),
            public_frame_node->GetPageNode());
  EXPECT_EQ(static_cast<const ProcessNode*>(frame_node->process_node()),
            public_frame_node->GetProcessNode());
  EXPECT_EQ(frame_node->frame_tree_node_id(),
            public_frame_node->GetFrameTreeNodeId());
  EXPECT_EQ(frame_node->dev_tools_token(),
            public_frame_node->GetDevToolsToken());
  EXPECT_EQ(frame_node->browsing_instance_id(),
            public_frame_node->GetBrowsingInstanceId());
  EXPECT_EQ(frame_node->site_instance_id(),
            public_frame_node->GetSiteInstanceId());

  auto child_frame_nodes = public_frame_node->GetChildFrameNodes();
  for (auto* child_frame_node : frame_node->child_frame_nodes()) {
    const FrameNode* child = child_frame_node;
    EXPECT_TRUE(base::Contains(child_frame_nodes, child));
  }
  EXPECT_EQ(child_frame_nodes.size(), frame_node->child_frame_nodes().size());

  EXPECT_EQ(frame_node->lifecycle_state(),
            public_frame_node->GetLifecycleState());
  EXPECT_EQ(frame_node->has_nonempty_beforeunload(),
            public_frame_node->HasNonemptyBeforeUnload());
  EXPECT_EQ(frame_node->url(), public_frame_node->GetURL());
  EXPECT_EQ(frame_node->is_current(), public_frame_node->IsCurrent());
  EXPECT_EQ(frame_node->network_almost_idle(),
            public_frame_node->GetNetworkAlmostIdle());
  EXPECT_EQ(frame_node->is_ad_frame(), public_frame_node->IsAdFrame());
}

}  // namespace performance_manager
