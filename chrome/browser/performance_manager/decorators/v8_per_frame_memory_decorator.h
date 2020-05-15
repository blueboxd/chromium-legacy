// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MANAGER_DECORATORS_V8_PER_FRAME_MEMORY_DECORATOR_H_
#define CHROME_BROWSER_PERFORMANCE_MANAGER_DECORATORS_V8_PER_FRAME_MEMORY_DECORATOR_H_

#include "base/time/time.h"
#include "chrome/common/performance_manager/mojom/v8_per_frame_memory.mojom.h"
#include "components/performance_manager/public/graph/graph.h"
#include "components/performance_manager/public/graph/node_data_describer.h"
#include "components/performance_manager/public/graph/process_node.h"

namespace performance_manager {

class V8PerFrameMemoryDecorator : public GraphOwned,
                                  public ProcessNode::ObserverDefaultImpl,
                                  public NodeDataDescriberDefaultImpl {
 public:
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

  uint64_t GetUnassociatedBytesForTesting(const ProcessNode* process_node);

 private:
  class ProcessData;
  class FrameData;

  friend class ProcessData;

  // Testing seam.
  virtual void BindReceiverWithProxyHost(
      mojo::PendingReceiver<
          performance_manager::mojom::V8PerFrameMemoryReporter>,
      RenderProcessHostProxy proxy) const;

  const base::TimeDelta min_time_between_requests_per_process_;
  Graph* graph_ = nullptr;
};

}  // namespace performance_manager

#endif  // CHROME_BROWSER_PERFORMANCE_MANAGER_DECORATORS_V8_PER_FRAME_MEMORY_DECORATOR_H_
