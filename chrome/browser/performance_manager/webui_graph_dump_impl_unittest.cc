// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/webui_graph_dump_impl.h"

#include <map>
#include <set>

#include "base/run_loop.h"
#include "base/stl_util.h"
#include "base/test/bind_test_util.h"
#include "base/time/time.h"
#include "chrome/browser/performance_manager/graph/mock_graphs.h"
#include "chrome/browser/performance_manager/graph/page_node_impl.h"
#include "chrome/browser/performance_manager/performance_manager_clock.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace performance_manager {

namespace {

class TestChangeStream : public mojom::WebUIGraphChangeStream {
 public:
  using FrameMap = std::map<int64_t, mojom::WebUIFrameInfoPtr>;
  using PageMap = std::map<int64_t, mojom::WebUIPageInfoPtr>;
  using ProcessMap = std::map<int64_t, mojom::WebUIProcessInfoPtr>;
  using IdSet = std::set<int64_t>;

  TestChangeStream() : binding_(this) {}

  mojom::WebUIGraphChangeStreamPtr GetProxy() {
    mojom::WebUIGraphChangeStreamPtr proxy;

    binding_.Bind(mojo::MakeRequest(&proxy));

    return proxy;
  }

  // mojom::WebUIGraphChangeStream implementation
  void FrameCreated(mojom::WebUIFrameInfoPtr frame) override {
    EXPECT_FALSE(HasId(frame->id));
    // If the node has a parent frame, we must have heard of it.
    EXPECT_TRUE(HasIdIfValid(frame->parent_frame_id));
    EXPECT_TRUE(HasId(frame->page_id));
    EXPECT_TRUE(HasId(frame->process_id));

    id_set_.insert(frame->id);
    frame_map_.insert(std::make_pair(frame->id, std::move(frame)));
  }

  void PageCreated(mojom::WebUIPageInfoPtr page) override {
    EXPECT_FALSE(HasId(page->id));
    id_set_.insert(page->id);
    page_map_.insert(std::make_pair(page->id, std::move(page)));
  }

  void ProcessCreated(mojom::WebUIProcessInfoPtr process) override {
    EXPECT_FALSE(HasId(process->id));
    id_set_.insert(process->id);
    process_map_.insert(std::make_pair(process->id, std::move(process)));
  }

  void FrameChanged(mojom::WebUIFrameInfoPtr frame) override {
    EXPECT_TRUE(HasId(frame->id));
    frame_map_[frame->id] = std::move(frame);
    ++num_changes_;
  }

  void PageChanged(mojom::WebUIPageInfoPtr page) override {
    EXPECT_TRUE(HasId(page->id));
    page_map_[page->id] = std::move(page);
    ++num_changes_;
  }

  void ProcessChanged(mojom::WebUIProcessInfoPtr process) override {
    EXPECT_TRUE(HasId(process->id));
    process_map_[process->id] = std::move(process);
    ++num_changes_;
  }

  void FavIconDataAvailable(mojom::WebUIFavIconInfoPtr favicon) override {}

  void NodeDeleted(int64_t node_id) override {
    EXPECT_EQ(1u, id_set_.erase(node_id));

    size_t erased = frame_map_.erase(node_id) + page_map_.erase(node_id) +
                    process_map_.erase(node_id);
    EXPECT_EQ(1u, erased);
  }

  const FrameMap& frame_map() const { return frame_map_; }
  const PageMap& page_map() const { return page_map_; }
  const ProcessMap& process_map() const { return process_map_; }
  const IdSet& id_set() const { return id_set_; }
  size_t num_changes() const { return num_changes_; }

 private:
  bool HasId(int64_t id) { return base::Contains(id_set_, id); }
  bool HasIdIfValid(int64_t id) { return id == 0u || HasId(id); }

  FrameMap frame_map_;
  PageMap page_map_;
  ProcessMap process_map_;
  IdSet id_set_;
  size_t num_changes_ = 0;

  mojo::Binding<mojom::WebUIGraphChangeStream> binding_;
};

}  // namespace

TEST(WebUIGraphDumpImplTest, ChangeStream) {
  content::TestBrowserThreadBundle browser_thread_bundle;

  GraphImpl graph;
  MockMultiplePagesWithMultipleProcessesGraph mock_graph(&graph);

  base::TimeTicks now = PerformanceManagerClock::NowTicks();

  const GURL kExampleUrl("http://www.example.org");
  mock_graph.page->OnMainFrameNavigationCommitted(now, 1, kExampleUrl);
  mock_graph.other_page->OnMainFrameNavigationCommitted(now, 2, kExampleUrl);

  auto* main_frame = mock_graph.page->GetMainFrameNodeImpl();
  main_frame->OnNavigationCommitted(kExampleUrl, /* same_document */ false);

  WebUIGraphDumpImpl impl(&graph);
  // Create a mojo proxy to the impl.
  mojom::WebUIGraphDumpPtr impl_proxy;
  impl.Bind(mojo::MakeRequest(&impl_proxy), base::OnceClosure());

  TestChangeStream change_stream;
  impl_proxy->SubscribeToChanges(change_stream.GetProxy());

  browser_thread_bundle.RunUntilIdle();

  // Validate that the initial graph state dump is complete.
  EXPECT_EQ(0u, change_stream.num_changes());
  EXPECT_EQ(7u, change_stream.id_set().size());

  EXPECT_EQ(2u, change_stream.process_map().size());
  for (const auto& kv : change_stream.process_map()) {
    EXPECT_NE(0u, kv.second->id);
  }

  EXPECT_EQ(3u, change_stream.frame_map().size());

  // Count the top-level frames as we go.
  size_t top_level_frames = 0;
  for (const auto& kv : change_stream.frame_map()) {
    const auto& frame = kv.second;
    if (frame->parent_frame_id == 0) {
      ++top_level_frames;

      // Top level frames should have a page ID.
      EXPECT_NE(0u, frame->page_id);

      // The page's main frame should have an URL.
      if (frame->id == NodeBase::GetSerializationId(main_frame))
        EXPECT_EQ(kExampleUrl, frame->url);
    }
    EXPECT_NE(0u, frame->id);
    EXPECT_NE(0u, frame->process_id);
  }

  // Make sure we have one top-level frame per page.
  EXPECT_EQ(change_stream.page_map().size(), top_level_frames);

  EXPECT_EQ(2u, change_stream.page_map().size());
  for (const auto& kv : change_stream.page_map()) {
    const auto& page = kv.second;
    EXPECT_NE(0u, page->id);
    EXPECT_EQ(kExampleUrl, page->main_frame_url);
  }

  // Test change notifications.
  const GURL kAnotherURL("http://www.google.com/");
  mock_graph.page->OnMainFrameNavigationCommitted(now, 1, kAnotherURL);

  size_t child_frame_id =
      NodeBase::GetSerializationId(mock_graph.child_frame.get());
  mock_graph.child_frame.reset();

  browser_thread_bundle.RunUntilIdle();

  EXPECT_EQ(1u, change_stream.num_changes());
  EXPECT_FALSE(base::Contains(change_stream.id_set(), child_frame_id));

  const auto main_page_it = change_stream.page_map().find(
      NodeBase::GetSerializationId(mock_graph.page.get()));
  ASSERT_TRUE(main_page_it != change_stream.page_map().end());
  EXPECT_EQ(kAnotherURL, main_page_it->second->main_frame_url);

  browser_thread_bundle.RunUntilIdle();
}

}  // namespace performance_manager
