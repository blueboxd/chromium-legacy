// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_GRAPH_NODE_DATA_DESCRIBER_H_
#define COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_GRAPH_NODE_DATA_DESCRIBER_H_

#include "base/values.h"

namespace performance_manager {

class FrameNode;
class PageNode;
class ProcessNode;
class SystemNode;
class WorkerNode;

// An interface for decoding node-private data for ultimate display as
// human-comprehensible text to allow diagnosis of node private data.
class NodeDataDescriber {
 public:
  virtual ~NodeDataDescriber() = default;

  virtual base::Value DescribeFrameNodeData(const FrameNode* node) const = 0;
  virtual base::Value DescribePageNodeData(const PageNode* node) const = 0;
  virtual base::Value DescribeProcessNodeData(
      const ProcessNode* node) const = 0;
  virtual base::Value DescribeSystemNodeData(const SystemNode* node) const = 0;
  virtual base::Value DescribeWorkerNodeData(const WorkerNode* node) const = 0;
};

// A convenience do-nothing implementation of the interface above. Returns
// an is_none() value for all nodes.
class NodeDataDescriberDefaultImpl : public NodeDataDescriber {
 public:
  base::Value DescribeFrameNodeData(const FrameNode* node) const override;
  base::Value DescribePageNodeData(const PageNode* node) const override;
  base::Value DescribeProcessNodeData(const ProcessNode* node) const override;
  base::Value DescribeSystemNodeData(const SystemNode* node) const override;
  base::Value DescribeWorkerNodeData(const WorkerNode* node) const override;
};

}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_GRAPH_NODE_DATA_DESCRIBER_H_
