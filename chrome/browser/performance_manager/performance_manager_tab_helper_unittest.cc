// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/performance_manager_tab_helper.h"

#include <set>

#include "base/run_loop.h"
#include "base/stl_util.h"
#include "base/test/bind_test_util.h"
#include "chrome/browser/performance_manager/graph/frame_node_impl.h"
#include "chrome/browser/performance_manager/graph/graph_impl_operations.h"
#include "chrome/browser/performance_manager/graph/page_node_impl.h"
#include "chrome/browser/performance_manager/performance_manager_test_harness.h"
#include "chrome/browser/performance_manager/render_process_user_data.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/test/navigation_simulator.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace performance_manager {

const char kParentUrl[] = "https://parent.com/";
const char kChild1Url[] = "https://child1.com/";
const char kChild2Url[] = "https://child2.com/";
const char kGrandchildUrl[] = "https://grandchild.com/";
const char kNewGrandchildUrl[] = "https://newgrandchild.com/";

class PerformanceManagerTabHelperTest : public PerformanceManagerTestHarness {
 public:
  PerformanceManagerTabHelperTest() = default;

  // A helper function for checking that the graph matches the topology of
  // stuff in content. The graph should reflect the set of processes provided
  // by |hosts|, even though content may actually have other processes lying
  // around.
  void CheckGraphTopology(const std::set<content::RenderProcessHost*>& hosts,
                          const char* grandchild_url);

 protected:
  static size_t CountAllRenderProcessHosts() {
    size_t num_hosts = 0;
    for (auto it = content::RenderProcessHost::AllHostsIterator();
         !it.IsAtEnd(); it.Advance()) {
      ++num_hosts;
    }

    return num_hosts;
  }
};

void PerformanceManagerTabHelperTest::CheckGraphTopology(
    const std::set<content::RenderProcessHost*>& hosts,
    const char* grandchild_url) {
  // There may be more RenderProcessHosts in existence than those used from
  // the RFHs above. The graph may not reflect all of them, as only those
  // observed through the TabHelper will have been reflected in the graph.
  size_t num_hosts = CountAllRenderProcessHosts();
  EXPECT_LE(hosts.size(), num_hosts);
  EXPECT_NE(0u, hosts.size());

  // Convert the RPHs to ProcessNodeImpls so we can check they match.
  std::set<ProcessNodeImpl*> process_nodes;
  for (auto* host : hosts) {
    auto* data = RenderProcessUserData::GetForRenderProcessHost(host);
    EXPECT_TRUE(data);
    process_nodes.insert(data->process_node());
  }
  EXPECT_EQ(process_nodes.size(), hosts.size());

  base::RunLoop run_loop;
  auto quit_closure = run_loop.QuitClosure();

  // Check out the graph itself.
  PerformanceManager::GetInstance()->CallOnGraph(
      FROM_HERE,
      base::BindLambdaForTesting([quit_closure, &process_nodes, num_hosts,
                                  grandchild_url](GraphImpl* graph) {
        EXPECT_GE(num_hosts, graph->GetAllProcessNodes().size());
        EXPECT_EQ(4u, graph->GetAllFrameNodes().size());

        // Expect all frame nodes to be current. This fails if our
        // implementation of RenderFrameHostChanged is borked.
        for (auto* frame : graph->GetAllFrameNodes())
          EXPECT_TRUE(frame->is_current());

        ASSERT_EQ(1u, graph->GetAllPageNodes().size());
        auto* page = graph->GetAllPageNodes()[0];

        // Extra RPHs can and most definitely do exist.
        auto associated_process_nodes =
            GraphImplOperations::GetAssociatedProcessNodes(page);
        EXPECT_GE(graph->GetAllProcessNodes().size(),
                  associated_process_nodes.size());
        EXPECT_GE(num_hosts, associated_process_nodes.size());

        for (auto* process_node : associated_process_nodes)
          EXPECT_TRUE(base::Contains(process_nodes, process_node));

        EXPECT_EQ(4u, GraphImplOperations::GetFrameNodes(page).size());
        ASSERT_EQ(1u, page->main_frame_nodes().size());

        auto* main_frame = page->GetMainFrameNode();
        EXPECT_EQ(kParentUrl, main_frame->url().spec());
        EXPECT_EQ(2u, main_frame->child_frame_nodes().size());

        for (auto* child_frame : main_frame->child_frame_nodes()) {
          if (child_frame->url().spec() == kChild1Url) {
            ASSERT_EQ(1u, child_frame->child_frame_nodes().size());
            auto* grandchild_frame =
                *(child_frame->child_frame_nodes().begin());
            EXPECT_EQ(grandchild_url, grandchild_frame->url().spec());
          } else if (child_frame->url().spec() == kChild2Url) {
            EXPECT_TRUE(child_frame->child_frame_nodes().empty());
          } else {
            FAIL() << "Unexpected child frame: " << child_frame->url().spec();
          }
        }

        quit_closure.Run();
      }));

  // Run until the graph checks have completed. This ensures lifetime safety of
  // the data structures passed into the closure.
  run_loop.Run();
}

TEST_F(PerformanceManagerTabHelperTest, FrameHierarchyReflectsToGraph) {
  SetContents(CreateTestWebContents());

  auto* parent = content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL(kParentUrl));
  DCHECK(parent);

  auto* parent_tester = content::RenderFrameHostTester::For(parent);
  auto* child1 = content::NavigationSimulator::NavigateAndCommitFromDocument(
      GURL(kChild1Url), parent_tester->AppendChild("child1"));
  auto* grandchild =
      content::NavigationSimulator::NavigateAndCommitFromDocument(
          GURL(kGrandchildUrl),
          content::RenderFrameHostTester::For(child1)->AppendChild(
              "grandchild"));
  auto* child2 = content::NavigationSimulator::NavigateAndCommitFromDocument(
      GURL(kChild2Url), parent_tester->AppendChild("child2"));

  // Count the RFHs referenced.
  std::set<content::RenderProcessHost*> hosts;
  auto* grandchild_process = grandchild->GetProcess();
  hosts.insert(main_rfh()->GetProcess());
  hosts.insert(child1->GetProcess());
  hosts.insert(grandchild->GetProcess());
  hosts.insert(child2->GetProcess());

  CheckGraphTopology(hosts, kGrandchildUrl);

  // Navigate the grand-child frame. This tests that we accurately observe the
  // new RFH being created and marked current, with the old one being marked not
  // current and torn down. Note that the old RPH doesn't get torn down.
  auto* new_grandchild =
      content::NavigationSimulator::NavigateAndCommitFromDocument(
          GURL(kNewGrandchildUrl), grandchild);
  auto* new_grandchild_process = new_grandchild->GetProcess();

  // Update the set of processes we expect to be associated with the page.
  hosts.erase(grandchild_process);
  hosts.insert(new_grandchild_process);

  CheckGraphTopology(hosts, kNewGrandchildUrl);

  // Clean up the web contents, which should dispose of the page and frame nodes
  // involved.
  DeleteContents();

  // Allow content/ to settle.
  thread_bundle()->RunUntilIdle();

  {
    size_t num_hosts = CountAllRenderProcessHosts();

    PerformanceManager::GetInstance()->CallOnGraph(
        FROM_HERE, base::BindLambdaForTesting([num_hosts](GraphImpl* graph) {
          EXPECT_GE(num_hosts, graph->GetAllProcessNodes().size());
          EXPECT_EQ(0u, graph->GetAllFrameNodes().size());
          ASSERT_EQ(0u, graph->GetAllPageNodes().size());
        }));

    thread_bundle()->RunUntilIdle();
  }

  // The RenderProcessHosts seem to get leaked, or at least be still alive here,
  // so explicitly detach from them in order to clean up the graph nodes.
  RenderProcessUserData::DetachAndDestroyAll();
}

}  // namespace performance_manager
