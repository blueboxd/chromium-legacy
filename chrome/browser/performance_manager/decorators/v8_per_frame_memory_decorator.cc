// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/decorators/v8_per_frame_memory_decorator.h"

#include "base/bind.h"
#include "base/check.h"
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/task/post_task.h"
#include "base/timer/timer.h"
#include "components/performance_manager/public/graph/frame_node.h"
#include "components/performance_manager/public/graph/node_attached_data.h"
#include "components/performance_manager/public/graph/node_data_describer_registry.h"
#include "components/performance_manager/public/graph/process_node.h"
#include "components/performance_manager/public/performance_manager.h"
#include "components/performance_manager/public/render_process_host_proxy.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/render_process_host.h"

namespace performance_manager {

class V8PerFrameMemoryDecorator::FrameData
    : public ExternalNodeAttachedDataImpl<
          V8PerFrameMemoryDecorator::FrameData> {
 public:
  explicit FrameData(const FrameNode* frame_node) {}
  ~FrameData() override = default;

  FrameData(const FrameData&) = delete;
  FrameData& operator=(const FrameData&) = delete;

  void set_v8_bytes_used(uint64_t v8_bytes_used) {
    v8_bytes_used_ = v8_bytes_used;
  }
  uint64_t v8_bytes_used() const { return v8_bytes_used_; }

 private:
  uint64_t v8_bytes_used_ = 0;
};

class V8PerFrameMemoryDecorator::ProcessData
    : public ExternalNodeAttachedDataImpl<
          V8PerFrameMemoryDecorator::ProcessData> {
 public:
  explicit ProcessData(const ProcessNode* process_node)
      : process_node_(process_node) {}
  ~ProcessData() override = default;

  ProcessData(const ProcessData&) = delete;
  ProcessData& operator=(const ProcessData&) = delete;

  void Initialize(const V8PerFrameMemoryDecorator* decorator);

  uint64_t unassociated_v8_bytes_used() const {
    return unassociated_v8_bytes_used_;
  }

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

  uint64_t unassociated_v8_bytes_used_ = 0;
};

void V8PerFrameMemoryDecorator::ProcessData::Initialize(
    const V8PerFrameMemoryDecorator* decorator) {
  DCHECK_EQ(nullptr, decorator_);
  decorator_ = decorator;

  StartMeasurement();
}

void V8PerFrameMemoryDecorator::ProcessData::StartMeasurement() {
  DCHECK(process_node_);

  last_request_time_ = base::TimeTicks::Now();

  EnsureRemote();
  resource_usage_reporter_->GetPerFrameV8MemoryUsageData(base::BindOnce(
      &V8PerFrameMemoryDecorator::ProcessData::OnPerFrameV8MemoryUsageData,
      base::Unretained(this)));
}

void V8PerFrameMemoryDecorator::ProcessData::ScheduleNextMeasurement() {
  base::TimeTicks next_request_time =
      last_request_time_ + decorator_->min_time_between_requests_per_process();

  timer_.Start(FROM_HERE, next_request_time - base::TimeTicks::Now(), this,
               &ProcessData::StartMeasurement);
}

void V8PerFrameMemoryDecorator::ProcessData::OnPerFrameV8MemoryUsageData(
    performance_manager::mojom::PerProcessV8MemoryUsageDataPtr result) {
  // Distribute the data to the frames.
  // If a frame doesn't have corresponding data in the result, clear any data
  // it may have had. Any datum in the result that doesn't correspond to an
  // existing frame is likewise accured to unassociated usage.
  unassociated_v8_bytes_used_ = result->unassociated_bytes_used;

  base::flat_map<base::UnguessableToken, mojom::PerFrameV8MemoryUsageDataPtr>
      associated_memory;
  associated_memory.swap(result->associated_memory);

  base::flat_set<const FrameNode*> frame_nodes = process_node_->GetFrameNodes();
  for (const FrameNode* frame_node : frame_nodes) {
    auto it = associated_memory.find(frame_node->GetDevToolsToken());
    if (it == associated_memory.end()) {
      // No data for this node, clear any data associated with it.
      FrameData::Destroy(frame_node);
    } else {
      // There should always be data for the main isolated world for each frame.
      DCHECK(base::Contains(it->second->associated_bytes, 0));

      FrameData* frame_data = FrameData::GetOrCreate(frame_node);
      for (const auto& kv : it->second->associated_bytes) {
        if (kv.first == 0) {
          frame_data->set_v8_bytes_used(kv.second->bytes_used);
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
    unassociated_v8_bytes_used_ += it.second->associated_bytes[0]->bytes_used;
  }

  // Schedule another measurement for this process node.
  ScheduleNextMeasurement();
}

void V8PerFrameMemoryDecorator::ProcessData::EnsureRemote() {
  if (resource_usage_reporter_.is_bound())
    return;

  mojo::PendingReceiver<performance_manager::mojom::V8PerFrameMemoryReporter>
      pending_receiver = resource_usage_reporter_.BindNewPipeAndPassReceiver();

  RenderProcessHostProxy proxy = process_node_->GetRenderProcessHostProxy();

  decorator_->BindReceiverWithProxyHost(std::move(pending_receiver), proxy);
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
  DCHECK_EQ(nullptr, V8PerFrameMemoryDecorator::ProcessData::Get(process_node));
  V8PerFrameMemoryDecorator::ProcessData* process_data =
      V8PerFrameMemoryDecorator::ProcessData::GetOrCreate(process_node);
  DCHECK_NE(nullptr, process_data);
  process_data->Initialize(this);
}

base::Value V8PerFrameMemoryDecorator::DescribeFrameNodeData(
    const FrameNode* frame_node) const {
  FrameData* frame_data = FrameData::Get(frame_node);
  if (!frame_data)
    return base::Value();

  base::Value dict(base::Value::Type::DICTIONARY);
  dict.SetIntKey("v8_bytes_used_", frame_data->v8_bytes_used());
  return dict;
}

base::Value V8PerFrameMemoryDecorator::DescribeProcessNodeData(
    const ProcessNode* process_node) const {
  ProcessData* process_data = ProcessData::Get(process_node);
  if (!process_data)
    return base::Value();

  base::Value dict(base::Value::Type::DICTIONARY);
  dict.SetIntKey("unassociated_v8_bytes_used_",
                 process_data->unassociated_v8_bytes_used());
  return dict;
}

uint64_t V8PerFrameMemoryDecorator::GetUnassociatedBytesForTesting(
    const ProcessNode* process_node) {
  ProcessData* process_data = ProcessData::Get(process_node);
  if (!process_data)
    return 0u;

  return process_data->unassociated_v8_bytes_used();
}

void V8PerFrameMemoryDecorator::BindReceiverWithProxyHost(
    mojo::PendingReceiver<performance_manager::mojom::V8PerFrameMemoryReporter>
        pending_receiver,
    RenderProcessHostProxy proxy) const {
  // Forward the pending receiver to the RenderProcessHost and bind it on the
  // UI thread.
  base::PostTask(
      FROM_HERE, {content::BrowserThread::UI},
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
