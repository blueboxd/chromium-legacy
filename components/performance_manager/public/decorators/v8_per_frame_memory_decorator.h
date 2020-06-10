// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_DECORATORS_V8_PER_FRAME_MEMORY_DECORATOR_H_
#define COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_DECORATORS_V8_PER_FRAME_MEMORY_DECORATOR_H_

#include "base/time/time.h"
#include "components/performance_manager/public/graph/graph.h"
#include "components/performance_manager/public/graph/node_data_describer.h"
#include "components/performance_manager/public/graph/process_node.h"
#include "content/public/common/performance_manager/v8_per_frame_memory.mojom.h"

namespace performance_manager {

namespace internal {
class ProxyHostReceiverBinder;
}

class V8PerFrameMemoryDecorator : public GraphOwned,
                                  public ProcessNode::ObserverDefaultImpl,
                                  public NodeDataDescriberDefaultImpl {
 public:
  class FrameData;
  class ProcessData;

  // Creates a new decorator with the given time between requests per process,
  // which bounds the number of requests over time.
  explicit V8PerFrameMemoryDecorator(
      base::TimeDelta min_time_between_requests_per_process);
  ~V8PerFrameMemoryDecorator() override;

  V8PerFrameMemoryDecorator(const V8PerFrameMemoryDecorator&) = delete;
  V8PerFrameMemoryDecorator& operator=(const V8PerFrameMemoryDecorator&) =
      delete;

  // GraphOwned implementation.
  void OnPassedToGraph(Graph* graph) override;
  void OnTakenFromGraph(Graph* graph) override;

  // ProcessNodeObserver overrides.
  void OnProcessNodeAdded(const ProcessNode* process_node) override;

  // NodeDataDescriber overrides.
  base::Value DescribeFrameNodeData(const FrameNode* node) const override;
  base::Value DescribeProcessNodeData(const ProcessNode* node) const override;

  base::TimeDelta min_time_between_requests_per_process() const {
    return min_time_between_requests_per_process_;
  }

 private:
  friend class internal::ProxyHostReceiverBinder;

  // Testing seam.
  virtual void BindReceiverWithProxyHost(
      mojo::PendingReceiver<
          performance_manager::mojom::V8PerFrameMemoryReporter>,
      RenderProcessHostProxy proxy) const;

  const base::TimeDelta min_time_between_requests_per_process_;
  Graph* graph_ = nullptr;
};

class V8PerFrameMemoryDecorator::FrameData {
 public:
  FrameData() = default;
  virtual ~FrameData() = default;

  FrameData(const FrameData&) = delete;
  FrameData& operator=(const FrameData&) = delete;

  // Returns the number of bytes used by V8 for this frame at the last
  // measurement.
  uint64_t v8_bytes_used() const { return v8_bytes_used_; }

  void set_v8_bytes_used(uint64_t v8_bytes_used) {
    v8_bytes_used_ = v8_bytes_used;
  }

  // Returns FrameData for the given node, or nullptr if no measurement has
  // been taken. The returned pointer must only be accessed on the graph
  // sequence and may go invalid at any time after leaving the calling scope.
  static const FrameData* ForFrameNode(const FrameNode* node);

 private:
  uint64_t v8_bytes_used_ = 0;
};

class V8PerFrameMemoryDecorator::ProcessData {
 public:
  ProcessData() = default;
  virtual ~ProcessData() = default;

  ProcessData(const ProcessData&) = delete;
  ProcessData& operator=(const ProcessData&) = delete;

  // Returns the number of bytes used by V8 at the last measurement in this
  // process that could not be attributed to a frame.
  uint64_t unassociated_v8_bytes_used() const {
    return unassociated_v8_bytes_used_;
  }

  void set_unassociated_v8_bytes_used(uint64_t unassociated_v8_bytes_used) {
    unassociated_v8_bytes_used_ = unassociated_v8_bytes_used;
  }

  // Returns FrameData for the given node, or nullptr if no measurement has
  // been taken. The returned pointer must only be accessed on the graph
  // sequence and may go invalid at any time after leaving the calling scope.
  static const ProcessData* ForProcessNode(const ProcessNode* node);

 private:
  uint64_t unassociated_v8_bytes_used_ = 0;
};

}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_DECORATORS_V8_PER_FRAME_MEMORY_DECORATOR_H_
