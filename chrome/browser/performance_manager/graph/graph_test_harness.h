// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MANAGER_GRAPH_GRAPH_TEST_HARNESS_H_
#define CHROME_BROWSER_PERFORMANCE_MANAGER_GRAPH_GRAPH_TEST_HARNESS_H_

#include <stdint.h>
#include <memory>
#include <string>
#include <utility>

#include "base/test/scoped_task_environment.h"
#include "chrome/browser/performance_manager/graph/frame_node_impl.h"
#include "chrome/browser/performance_manager/graph/graph_impl.h"
#include "chrome/browser/performance_manager/graph/node_base.h"
#include "chrome/browser/performance_manager/graph/page_node_impl.h"
#include "chrome/browser/performance_manager/graph/process_node_impl.h"
#include "chrome/browser/performance_manager/graph/system_node_impl.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace performance_manager {

template <class NodeClass>
class TestNodeWrapper {
 public:
  struct Factory;

  template <typename... Args>
  static TestNodeWrapper<NodeClass> Create(GraphImpl* graph, Args&&... args);

  TestNodeWrapper() {}

  explicit TestNodeWrapper(std::unique_ptr<NodeClass> impl)
      : impl_(std::move(impl)) {
    DCHECK(impl_.get());
  }

  TestNodeWrapper(TestNodeWrapper&& other) : impl_(std::move(other.impl_)) {}

  void operator=(TestNodeWrapper&& other) { impl_ = std::move(other.impl_); }
  void operator=(TestNodeWrapper& other) = delete;

  ~TestNodeWrapper() { reset(); }

  NodeClass* operator->() const { return impl_.get(); }

  NodeClass* get() const { return impl_.get(); }

  void reset() {
    if (impl_) {
      impl_->graph()->RemoveNode(impl_.get());
      impl_.reset();
    }
  }

 private:
  std::unique_ptr<NodeClass> impl_;
};

template <class NodeClass>
struct TestNodeWrapper<NodeClass>::Factory {
  template <typename... Args>
  static std::unique_ptr<NodeClass> Create(GraphImpl* graph, Args&&... args) {
    return std::make_unique<NodeClass>(graph, std::forward<Args>(args)...);
  }
};

// A specialized factory function for frame nodes that helps fill out some
// common values.
template <>
struct TestNodeWrapper<FrameNodeImpl>::Factory {
  static std::unique_ptr<FrameNodeImpl> Create(
      GraphImpl* graph,
      ProcessNodeImpl* process_node,
      PageNodeImpl* page_node,
      FrameNodeImpl* parent_frame_node = nullptr,
      int frame_tree_node_id = 0,
      const base::UnguessableToken& token = base::UnguessableToken::Create(),
      int32_t browsing_instance_id = 0,
      int32_t site_instance_id = 0) {
    return std::make_unique<FrameNodeImpl>(
        graph, process_node, page_node, parent_frame_node, frame_tree_node_id,
        token, browsing_instance_id, site_instance_id);
  }
};

// A specialized factory function for page nodes that helps fill out some
// common values.
template <>
struct TestNodeWrapper<PageNodeImpl>::Factory {
  static std::unique_ptr<PageNodeImpl> Create(
      GraphImpl* graph,
      const WebContentsProxy& wc_proxy = WebContentsProxy(),
      bool is_visible = false,
      bool is_audible = false) {
    return std::make_unique<PageNodeImpl>(graph, wc_proxy, is_visible,
                                          is_audible);
  }
};

// static
template <typename NodeClass>
template <typename... Args>
TestNodeWrapper<NodeClass> TestNodeWrapper<NodeClass>::Create(GraphImpl* graph,
                                                              Args&&... args) {
  // Dispatch to a helper so that we can use partial specialization.
  std::unique_ptr<NodeClass> node =
      Factory::Create(graph, std::forward<Args>(args)...);
  graph->AddNewNode(node.get());
  return TestNodeWrapper<NodeClass>(std::move(node));
}

// This specialization is necessary because the graph has ownership of the
// system node as it's a singleton. For the other node types the test wrapper
// manages the node lifetime.
template <>
class TestNodeWrapper<SystemNodeImpl> {
 public:
  static TestNodeWrapper<SystemNodeImpl> Create(GraphImpl* graph) {
    return TestNodeWrapper<SystemNodeImpl>(graph->FindOrCreateSystemNodeImpl());
  }

  explicit TestNodeWrapper(SystemNodeImpl* impl) : impl_(impl) {}
  TestNodeWrapper(TestNodeWrapper&& other) : impl_(other.impl_) {}
  ~TestNodeWrapper() { reset(); }

  SystemNodeImpl* operator->() const { return impl_; }
  SystemNodeImpl* get() const { return impl_; }

  void reset() { impl_ = nullptr; }

 private:
  SystemNodeImpl* impl_;

  DISALLOW_COPY_AND_ASSIGN(TestNodeWrapper);
};

class GraphTestHarness : public ::testing::Test {
 public:
  GraphTestHarness();
  ~GraphTestHarness() override;

  // Optional constructor for directly configuring the ScopedTaskEnvironment.
  template <class... ArgTypes>
  explicit GraphTestHarness(ArgTypes... args) : task_env_(args...) {}

  template <class NodeClass, typename... Args>
  TestNodeWrapper<NodeClass> CreateNode(Args&&... args) {
    return TestNodeWrapper<NodeClass>::Create(graph(),
                                              std::forward<Args>(args)...);
  }

  TestNodeWrapper<SystemNodeImpl> GetSystemNode() {
    return TestNodeWrapper<SystemNodeImpl>(
        graph()->FindOrCreateSystemNodeImpl());
  }

  // testing::Test:
  void TearDown() override;

 protected:
  base::test::ScopedTaskEnvironment& task_env() { return task_env_; }
  GraphImpl* graph() { return &graph_; }

 private:
  base::test::ScopedTaskEnvironment task_env_;
  GraphImpl graph_;
};

}  // namespace performance_manager

#endif  // CHROME_BROWSER_PERFORMANCE_MANAGER_GRAPH_GRAPH_TEST_HARNESS_H_
