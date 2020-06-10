// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/public/decorators/v8_per_frame_memory_decorator.h"

#include "base/bind.h"
#include "base/check.h"
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/timer/timer.h"
#include "components/performance_manager/public/graph/frame_node.h"
#include "components/performance_manager/public/graph/node_attached_data.h"
#include "components/performance_manager/public/graph/node_data_describer_registry.h"
#include "components/performance_manager/public/graph/process_node.h"
#include "components/performance_manager/public/performance_manager.h"
#include "components/performance_manager/public/render_process_host_proxy.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"

namespace performance_manager {

namespace internal {

// Provides access to V8PerFrameMemoryDecorator's private
// BindReceiverWithProxyHost method.
class ProxyHostReceiverBinder {
 public:
  static void Bind(const V8PerFrameMemoryDecorator& decorator,
                   mojo::PendingReceiver<
                       performance_manager::mojom::V8PerFrameMemoryReporter>
                       pending_receiver,
                   RenderProcessHostProxy proxy) {
    decorator.BindReceiverWithProxyHost(std::move(pending_receiver), proxy);
  }
};

}  // namespace internal

namespace {

// Private implementations of the node attached data. This keeps the complexity
// out of the header file.

class NodeAttachedFrameData
    : public ExternalNodeAttachedDataImpl<NodeAttachedFrameData> {
 public:
  explicit NodeAttachedFrameData(const FrameNode* frame_node) {}
  ~NodeAttachedFrameData() override = default;

  NodeAttachedFrameData(const NodeAttachedFrameData&) = delete;
  NodeAttachedFrameData& operator=(const NodeAttachedFrameData&) = delete;

  const V8PerFrameMemoryDecorator::FrameData* data() const {
    return data_available_ ? &data_ : nullptr;
  }

 private:
  friend class NodeAttachedProcessData;

  V8PerFrameMemoryDecorator::FrameData data_;
  bool data_available_ = false;
};

class NodeAttachedProcessData
    : public ExternalNodeAttachedDataImpl<NodeAttachedProcessData> {
 public:
  explicit NodeAttachedProcessData(const ProcessNode* process_node)
      : process_node_(process_node) {}
  ~NodeAttachedProcessData() override = default;

  NodeAttachedProcessData(const NodeAttachedProcessData&) = delete;
  NodeAttachedProcessData& operator=(const NodeAttachedProcessData&) = delete;

  const V8PerFrameMemoryDecorator::ProcessData* data() const {
    return data_available_ ? &data_ : nullptr;
  }

  void Initialize(const V8PerFrameMemoryDecorator* decorator);

 private:
  void StartMeasurement();
  void ScheduleNextMeasurement();
  void EnsureRemote();
  void OnPerFrameV8MemoryUsageData(
      performance_manager::mojom::PerProcessV8MemoryUsageDataPtr result);

  const ProcessNode* const process_node_;
  const V8PerFrameMemoryDecorator* decorator_ = nullptr;

  mojo::Remote<performance_manager::mojom::V8PerFrameMemoryReporter>
      resource_usage_reporter_;

  // Used to schedule the next measurement.
  base::TimeTicks last_request_time_;
  base::OneShotTimer timer_;

  V8PerFrameMemoryDecorator::ProcessData data_;
  bool data_available_ = false;
};

void NodeAttachedProcessData::Initialize(
    const V8PerFrameMemoryDecorator* decorator) {
  DCHECK_EQ(nullptr, decorator_);
  decorator_ = decorator;

  StartMeasurement();
}

void NodeAttachedProcessData::StartMeasurement() {
  last_request_time_ = base::TimeTicks::Now();

  EnsureRemote();
  resource_usage_reporter_->GetPerFrameV8MemoryUsageData(
      base::BindOnce(&NodeAttachedProcessData::OnPerFrameV8MemoryUsageData,
                     base::Unretained(this)));
}

void NodeAttachedProcessData::ScheduleNextMeasurement() {
  DCHECK_NE(nullptr, decorator_);
  base::TimeTicks next_request_time =
      last_request_time_ + decorator_->min_time_between_requests_per_process();

  timer_.Start(FROM_HERE, next_request_time - base::TimeTicks::Now(), this,
               &NodeAttachedProcessData::StartMeasurement);
}

void NodeAttachedProcessData::OnPerFrameV8MemoryUsageData(
    performance_manager::mojom::PerProcessV8MemoryUsageDataPtr result) {
  // Distribute the data to the frames.
  // If a frame doesn't have corresponding data in the result, clear any data
  // it may have had. Any datum in the result that doesn't correspond to an
  // existing frame is likewise accured to unassociated usage.
  uint64_t unassociated_v8_bytes_used = result->unassociated_bytes_used;

  base::flat_map<base::UnguessableToken, mojom::PerFrameV8MemoryUsageDataPtr>
      associated_memory;
  associated_memory.swap(result->associated_memory);

  base::flat_set<const FrameNode*> frame_nodes = process_node_->GetFrameNodes();
  for (const FrameNode* frame_node : frame_nodes) {
    auto it = associated_memory.find(frame_node->GetDevToolsToken());
    if (it == associated_memory.end()) {
      // No data for this node, clear any data associated with it.
      NodeAttachedFrameData::Destroy(frame_node);
    } else {
      // There should always be data for the main isolated world for each frame.
      DCHECK(base::Contains(it->second->associated_bytes, 0));

      NodeAttachedFrameData* frame_data =
          NodeAttachedFrameData::GetOrCreate(frame_node);
      for (const auto& kv : it->second->associated_bytes) {
        if (kv.first == 0) {
          frame_data->data_available_ = true;
          frame_data->data_.set_v8_bytes_used(kv.second->bytes_used);
        } else {
          // TODO(siggi): Where to stash the rest of the data?
        }
      }

      // Clear this datum as its usage has been consumed.
      associated_memory.erase(it);
    }
  }

  for (const auto& it : associated_memory) {
    // Accrue the data for non-existent frames to unassociated bytes.
    unassociated_v8_bytes_used += it.second->associated_bytes[0]->bytes_used;
  }

  data_available_ = true;
  data_.set_unassociated_v8_bytes_used(unassociated_v8_bytes_used);

  // Schedule another measurement for this process node.
  ScheduleNextMeasurement();
}

void NodeAttachedProcessData::EnsureRemote() {
  if (resource_usage_reporter_.is_bound())
    return;

  // This interface is implemented in //content/renderer/performance_manager.
  mojo::PendingReceiver<performance_manager::mojom::V8PerFrameMemoryReporter>
      pending_receiver = resource_usage_reporter_.BindNewPipeAndPassReceiver();

  RenderProcessHostProxy proxy = process_node_->GetRenderProcessHostProxy();

  DCHECK_NE(nullptr, decorator_);
  internal::ProxyHostReceiverBinder::Bind(*decorator_,
                                          std::move(pending_receiver), proxy);
}

}  // namespace

const V8PerFrameMemoryDecorator::FrameData*
V8PerFrameMemoryDecorator::FrameData::ForFrameNode(const FrameNode* node) {
  auto* node_data = NodeAttachedFrameData::Get(node);
  return node_data ? node_data->data() : nullptr;
}

const V8PerFrameMemoryDecorator::ProcessData*
V8PerFrameMemoryDecorator::ProcessData::ForProcessNode(
    const ProcessNode* node) {
  auto* node_data = NodeAttachedProcessData::Get(node);
  return node_data ? node_data->data() : nullptr;
}

V8PerFrameMemoryDecorator::V8PerFrameMemoryDecorator(
    base::TimeDelta min_time_between_requests_per_process)
    : min_time_between_requests_per_process_(
          min_time_between_requests_per_process) {}

V8PerFrameMemoryDecorator::~V8PerFrameMemoryDecorator() = default;

void V8PerFrameMemoryDecorator::OnPassedToGraph(Graph* graph) {
  graph_ = graph;

  // Iterate over the existing process nodes to put them under observation.
  for (const ProcessNode* process_node : graph->GetAllProcessNodes())
    OnProcessNodeAdded(process_node);

  graph->AddProcessNodeObserver(this);
  graph->GetNodeDataDescriberRegistry()->RegisterDescriber(
      this, "V8PerFrameMemoryDecorator");
}

void V8PerFrameMemoryDecorator::OnTakenFromGraph(Graph* graph) {
  graph->GetNodeDataDescriberRegistry()->UnregisterDescriber(this);
  graph->RemoveProcessNodeObserver(this);
  graph_ = nullptr;
}

void V8PerFrameMemoryDecorator::OnProcessNodeAdded(
    const ProcessNode* process_node) {
  DCHECK_EQ(nullptr, NodeAttachedProcessData::Get(process_node));

  // Only renderer processes have frames. Don't attempt to connect to other
  // process types.
  if (process_node->GetProcessType() != content::PROCESS_TYPE_RENDERER)
    return;

  NodeAttachedProcessData* process_data =
      NodeAttachedProcessData::GetOrCreate(process_node);
  process_data->Initialize(this);
}

base::Value V8PerFrameMemoryDecorator::DescribeFrameNodeData(
    const FrameNode* frame_node) const {
  const FrameData* const frame_data = FrameData::ForFrameNode(frame_node);
  if (!frame_data)
    return base::Value();

  base::Value dict(base::Value::Type::DICTIONARY);
  dict.SetIntKey("v8_bytes_used", frame_data->v8_bytes_used());
  return dict;
}

base::Value V8PerFrameMemoryDecorator::DescribeProcessNodeData(
    const ProcessNode* process_node) const {
  const ProcessData* const process_data =
      ProcessData::ForProcessNode(process_node);
  if (!process_data)
    return base::Value();

  DCHECK_EQ(content::PROCESS_TYPE_RENDERER, process_node->GetProcessType());

  base::Value dict(base::Value::Type::DICTIONARY);
  dict.SetIntKey("unassociated_v8_bytes_used",
                 process_data->unassociated_v8_bytes_used());
  return dict;
}

void V8PerFrameMemoryDecorator::BindReceiverWithProxyHost(
    mojo::PendingReceiver<performance_manager::mojom::V8PerFrameMemoryReporter>
        pending_receiver,
    RenderProcessHostProxy proxy) const {
  // Forward the pending receiver to the RenderProcessHost and bind it on the
  // UI thread.
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](RenderProcessHostProxy proxy,
             mojo::PendingReceiver<
                 performance_manager::mojom::V8PerFrameMemoryReporter>&&
                 pending_receiver) {
            if (!proxy.Get())
              return;

            proxy.Get()->BindReceiver(std::move(pending_receiver));
          },
          proxy, std::move(pending_receiver)));
}

}  // namespace performance_manager
